#!/usr/bin/env python3
"""Observe live source boundaries; runtime acceptance separately consumes fresh CTest results."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
LAYERS = ('Core', 'Application', 'Infrastructure', 'GUI', 'Compatibility', 'Composition')
ALLOWED = {
    'Core': set(), 'Application': {'Core'}, 'Infrastructure': {'Core', 'Application'},
    'GUI': {'Core', 'Application', 'Infrastructure', 'Compatibility'},
    'Compatibility': {'Core', 'Application'},
    'Composition': {'Core', 'Application', 'Infrastructure', 'GUI', 'Compatibility'},
}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def layer(name):
    if name.startswith('apps/'):
        return 'Composition'
    for prefix, value in [('src/core/', 'Core'), ('src/application/', 'Application'),
                          ('src/infrastructure/', 'Infrastructure'), ('src/gui/', 'GUI'),
                          ('src/compat/', 'Compatibility')]:
        if name.startswith(prefix):
            return value
    return None

def include_layer(name):
    for prefix, value in [('core/', 'Core'), ('qualpf/application/', 'Application'),
                          ('application/', 'Application'), ('infrastructure/', 'Infrastructure'),
                          ('gui/', 'GUI'), ('compat/', 'Compatibility')]:
        if name.startswith(prefix):
            return value
    if name.startswith('qualpf/'):
        return 'Core'
    return None

def strip_comments(source):
    return re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)

def acyclic(edges):
    graph = {item: set() for item in LAYERS}
    for origin, target in edges:
        if origin != target:
            graph[origin].add(target)
    seen, active = set(), set()
    def visit(node):
        if node in active:
            return False
        if node in seen:
            return True
        active.add(node)
        if not all(visit(target) for target in graph[node]):
            return False
        active.remove(node)
        seen.add(node)
        return True
    return all(visit(node) for node in graph)

def source_manifest(root=ROOT):
    # Documentation/evidence closure is committed separately from implementation.
    prefixes = ('src/', 'apps/', 'cmake/', 'tests/', 'third_party/', 'configured/', 'tools/', 'share/', 'deploy/')
    exact = {'CMakeLists.txt', 'CMakePresets.json', '.gitattributes', 'AGENTS.md'}
    try:
        names = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard'],
                                        cwd=root, text=True, encoding='utf-8', stderr=subprocess.DEVNULL).splitlines()
    except (OSError, subprocess.CalledProcessError):
        # Source archives have no .git directory but still run the live gate.
        # Python imports can create bytecode during testing. Git excludes these
        # through .gitignore; archive discovery must not treat runtime caches as
        # newly added source inputs. Real source/reference files remain bound.
        names = [path.relative_to(root).as_posix() for prefix in prefixes
                 for path in (root / prefix).rglob('*')
                 if path.is_file() and path.suffix not in ('.pyc', '.pyo')]
        names += [name for name in exact if (root / name).is_file()]
    return {name: sha(root / name) for name in sorted(set(names))
            if (name.startswith(prefixes) or name in exact) and (root / name).is_file()}

def combined_preprocessing_call(code):
    return bool(re.search(r'\b(?:extract_experimental_peaks|find_peaks)\s*\(', strip_comments(code)))


def algorithm_assembly_fields(code):
    fields = {}
    for name, pattern in [('AlgorithmSelections', r'AlgorithmIdentity\s+(\w+)\s*\{'),
                          ('AlgorithmModules', r'shared_ptr<[^;]+>\s+(\w+)\s*;')]:
        body = re.search(r'struct\s+' + name + r'\s*\{(.*?)\n\};', code, re.S)
        fields[name] = set(re.findall(pattern, body[1])) if body else set()
    return fields


def observe(root=ROOT):
    failures, edges, dependencies, qt_files, owners, engines = [], set(), [], [], [], []
    for folder in ('src', 'apps'):
        for path in sorted((root / folder).rglob('*')):
            if path.suffix not in ('.h', '.cpp'):
                continue
            name = path.relative_to(root).as_posix()
            origin = layer(name)
            if not origin:
                continue
            text = path.read_text(encoding='utf-8-sig')
            code = strip_comments(text)
            for include in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', code, re.M):
                target = include_layer(include)
                if target and target != origin:
                    edges.add((origin, target))
                    dependencies.append({'source': name, 'include': include, 'layer': target})
                    if target not in ALLOWED[origin]:
                        failures.append(f'Forbidden dependency: {name} -> {include}')
                if origin in ('Core', 'Application') and re.match(r'Q[A-Z]|Qt[A-Z]|Qt/', include):
                    qt_files.append(name)
                    failures.append(f'Qt include in {origin}: {name}')
            if origin == 'Infrastructure' and re.search(r'\b(?:QWidget|QMessageBox|QApplication|QMainWindow)\b', code):
                failures.append('GUI implementation in Infrastructure: ' + name)
            if re.search(r'\bProductSearchSession\s+\w+\s*;', code):
                owners.append(name)
            if re.search(r'\bstatic\b[^;{}\n]*\b(?:Workspace|WorkspaceContext|ProductSearchSession)\b', code):
                failures.append('Static product owner/context: ' + name)
            if 'AppState::' in code or 'gui/state/appstate.h' in code:
                failures.append('Production AppState facade: ' + name)
            if re.search(r'\bSearchEngine\s*\{[^{}]*\}\s*\.\s*run\s*\(', code):
                engines.append(name)
            if origin == 'Infrastructure' and re.search(r'\b(?:SessionView|ProductSearchSession|stop_source|setMinFom|setWeightPhases|classicScore)\b', code):
                failures.append('State/scoring/cancellation owner in Infrastructure: ' + name)
            if origin == 'Application' and combined_preprocessing_call(code):
                failures.append('Combined preprocessing orchestration in Application: ' + name)
            if origin in ('Core', 'Application') and re.search(r'\b(?:ifstream|ofstream|std::filesystem|QSqlDatabase)\b', code):
                failures.append('Concrete IO in pure layer: ' + name)
    assembly = root / 'src/application/include/qualpf/application/algorithms.h'
    if assembly.is_file():
        expected_modules = {'background', 'initial_peaks', 'fwhm', 'experimental_peaks', 'matcher',
                            'profile_expander', 'profile_fitter'}
        for owner, fields in algorithm_assembly_fields(assembly.read_text(encoding='utf-8')).items():
            if fields != expected_modules:
                failures.append('Unexpected scientific replacement points in ' + owner + ': ' + repr(sorted(fields)))
    search_files = ['src/gui/mainwindow.cpp', 'src/gui/mainwindow.h'] + [
        path.relative_to(root).as_posix() for path in (root / 'src/gui/search').glob('*')
        if path.suffix in ('.cpp', '.h')]
    forbidden = re.compile(r'\b(?:processEvents|waitForDone|waitForFinished|terminate)\s*\(')
    pumps = [name for name in search_files if forbidden.search(strip_comments((root / name).read_text(encoding='utf-8')))]
    failures.extend('Search event pump/blocking drain: ' + name for name in pumps)
    if owners != ['src/application/include/qualpf/application/search_coordinator.h']:
        failures.append('Unexpected canonical product owners: ' + repr(owners))
    if not acyclic(edges):
        failures.append('Layer dependency cycle')
    expected = {'src/application/session/reference_database.cpp', 'src/gui/search/search_task_controller.cpp'}
    if set(engines) != expected:
        failures.append('Unexpected SearchEngine execution callers: ' + repr(engines))
    if (root / 'src/gui/state/appstate.h').exists() or (root / 'src/gui/state/appstate.cpp').exists():
        failures.append('Production AppState files remain')
    if (root / 'cmake/modules/QualpfCompatibilityTargets.cmake').exists():
        failures.append('Empty Fortran target facade remains')
    infrastructure_cmake = (root / 'src/infrastructure/CMakeLists.txt').read_text(encoding='utf-8')
    if not re.search(r'qualpf_configure_qt_boundary\(qualpf_infrastructure Core Sql\)', infrastructure_cmake):
        failures.append('Infrastructure Qt modules are not explicitly Core/Sql')
    return dict(schema='qualpf.source-observations.v1', status='FAIL' if failures else 'PASS',
                failures=failures, layer_edges=[list(edge) for edge in sorted(edges)],
                include_dependencies=dependencies, qt_files_in_pure_layers=qt_files,
                canonical_owner_declarations=owners, search_engine_callers=engines,
                search_event_pumps=pumps, source_sha256=source_manifest(root))

def self_test():
    import tempfile
    from unittest.mock import patch
    with tempfile.TemporaryDirectory() as temporary:
        archive = Path(temporary)
        source_names = {'src/owned.cpp', 'tools/check.py', 'tests/data/reference.cif'}
        cache_names = {'tools/__pycache__/check.cpython-314.pyc', 'tools/check.pyc', 'tools/check.pyo'}
        for name in source_names | cache_names:
            path = archive / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'original')
        # Force the archive path even if the test temporary directory happens
        # to be inside an unrelated Git checkout.
        with patch.object(subprocess, 'check_output', side_effect=OSError('No Git metadata')):
            initial = source_manifest(archive)
            assert set(initial) == source_names
            (archive / 'tests/data/reference.cif').write_bytes(b'changed')
            changed = source_manifest(archive)
            assert changed['tests/data/reference.cif'] != initial['tests/data/reference.cif']
            (archive / 'src/added.cpp').write_bytes(b'new source')
            assert 'src/added.cpp' in source_manifest(archive)
            (archive / 'src/owned.cpp').unlink()
            assert 'src/owned.cpp' not in source_manifest(archive)
    engine_call = r'\bSearchEngine\s*\{[^{}]*\}\s*\.\s*run\s*\('
    assert re.search(engine_call, 'SearchEngine{}.run(input, reader, context)')
    assert re.search(engine_call, 'SearchEngine{task.algorithms}.run(input, reader, context)')
    assert not re.search(engine_call, 'SearchEngine{}.score(state, card, parameters, context)')
    assert include_layer('gui/state/workspace_context.h') not in ALLOWED['Infrastructure']
    assert include_layer('infrastructure/settings/search_settings.h') not in ALLOWED['Application']
    assert include_layer('qualpf/domain/result.h') in ALLOWED['Application']
    assert not acyclic({('Application', 'Core'), ('Core', 'Application')})
    assert acyclic({('GUI', 'Infrastructure'), ('Infrastructure', 'Application'), ('Application', 'Core')})
    assert combined_preprocessing_call('qualpf::preprocessing::find_peaks(pattern, options, wave, tolerance)')
    assert combined_preprocessing_call('preprocessing::extract_experimental_peaks(pattern, options, wave, tolerance)')
    assert not combined_preprocessing_call('evaluateFormalPeakExtraction(source, request, context)')
    assert not combined_preprocessing_call('// find_peaks(pattern)')
    fields = algorithm_assembly_fields('struct AlgorithmModules {\n std::shared_ptr<const search::IRirQuantifier> rir;\n};')
    assert fields['AlgorithmModules'] == {'rir'}
    print('PASS: archive source/cache distinction, forbidden dependencies and cycle negative controls')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
    result = observe()
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(result['status'] + ': Current source boundaries; ' + str(len(result['source_sha256'])) + ' bound inputs')
    for failure in result['failures']:
        print(failure)
    return bool(result['failures'])

if __name__ == '__main__':
    raise SystemExit(main())
