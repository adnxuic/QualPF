#!/usr/bin/env python3
"""Enforce live header provenance, Core operation ownership and configured links.

The original boundary checker remains a separate required check. This stricter
gate does not reinterpret the frozen Milestone A result.
"""
import argparse
import json
from pathlib import Path
import re
import tempfile

from check_boundaries import ROOT, layer, observe, strip_comments

ALLOWED = {
    'Core': {'Core'}, 'Application': {'Application', 'Core'},
    'Infrastructure': {'Infrastructure', 'Application', 'Core'},
    'GUI': {'GUI', 'Application'},
    'Composition': {'Composition', 'GUI', 'Infrastructure', 'Application', 'Core'},
}
QT = re.compile(r'^(?:Q[A-Z]\w*|Qt\w*/|Qt/)')
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"\n]+)[>"]', re.M)


def parts(value):
    # CMake directory-id wrappers have no semantic dependency. LINK_ONLY and
    # BUILD_INTERFACE retain the target/path they make visible to this build.
    value = re.sub(r'::@[^;]*', '', value or '')
    value = re.sub(r'\$<(?:LINK_ONLY|BUILD_INTERFACE):([^<>]*)>', r'\1', value)
    value = re.sub(r'\$<INSTALL_INTERFACE:[^<>]*>', '', value)
    result = []
    for item in value.split(';'):
        if not item:
            continue
        item = re.sub(r'qualpf::(\w+)', r'qualpf_\1', item)
        if '$<' in item:
            # Conservatively inspect every branch of conditional link expressions.
            result.extend(re.findall(r'(?:qualpf_\w+|Qt6::\w+)', item))
        else:
            result.append(item)
    return result


def relative(path, root):
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return ''


def target_layer(target, root):
    directory = Path(target.get('SOURCE_DIR', ''))
    name = relative(directory, root) + '/'
    if name.startswith('tests/') or name.startswith('benchmarks/'):
        return 'Test'
    return layer(name)


def graph_failures(targets, root):
    failures = []
    layers = {name: target_layer(item, root) for name, item in targets.items()}
    edges = {name: set(parts(item.get('LINK_LIBRARIES', '')) +
                       parts(item.get('INTERFACE_LINK_LIBRARIES', '')))
             for name, item in targets.items()}
    for name, origin in layers.items():
        if origin not in ALLOWED:
            continue
        for include in parts(targets[name].get('INTERFACE_INCLUDE_DIRECTORIES', '')):
            if Path(include).resolve() in (root.resolve(), (root / 'src').resolve()):
                failures.append(f'Broad public include export: {name} -> {include}')
        # Traversal carries whether Application has mediated access to Core.
        pending = [(link, origin == 'Application') for link in edges[name]]
        seen = set()
        while pending:
            dependency, mediated = pending.pop()
            if (dependency, mediated) in seen:
                continue
            seen.add((dependency, mediated))
            target = layers.get(dependency)
            if target == 'Application':
                mediated = True
            if dependency.startswith('Qt6::'):
                module = dependency.split('::')[-1]
                if origin in ('Core', 'Application') or (
                    origin == 'GUI' and module in ('Sql', 'Network')) or (
                    origin == 'Infrastructure' and module in ('Gui', 'Widgets', 'PrintSupport')):
                    failures.append(f'Forbidden Qt link: {name} -> {dependency}')
            elif target == 'Test' or 'compat' in dependency.lower() or 'snapshot' in dependency.lower():
                failures.append(f'Test/compat dependency in production: {name} -> {dependency}')
            elif target and target != origin:
                allowed = target in ALLOWED[origin] or (origin == 'GUI' and target == 'Core' and mediated)
                if not allowed:
                    failures.append(f'Forbidden target dependency: {name} -> {dependency} ({target})')
            # A facade with no layer cannot hide its product/Qt dependencies.
            pending.extend((link, mediated) for link in edges.get(dependency, ()))
    return sorted(set(failures))


def core_operations(root):
    """Index free functions by their declaring namespace; value types are legal."""
    functions = set()
    for path in (root / 'src/core').rglob('*.h'):
        code = strip_comments(path.read_text(encoding='utf-8-sig'))
        code = re.sub(r'^\s*#.*$', '', code, flags=re.M)
        stack, statement = [], ''
        for token in re.split(r'([{};])', code):
            if token not in ('{', '}', ';'):
                statement += token
                continue
            namespaces = [item for item in stack if item is not None]
            if all(item is not None for item in stack):
                match = re.search(r'\b([a-zA-Z_]\w*)\s*\([^{};]*\)\s*(?:noexcept)?\s*$', statement)
                if match and namespaces and match[1] not in ('static_assert', 'decltype', 'sizeof', 'alignof', 'if', 'switch'):
                    functions.add('::'.join(namespaces + [match[1]]))
            if token == '{':
                match = re.search(r'\bnamespace\s+([\w:]+)\s*$', statement)
                stack.append(match[1] if match else None)
            elif token == '}' and stack:
                stack.pop()
            statement = ''
    return functions


def operation_failures(code, name, operations):
    aliases = re.findall(r'\bnamespace\s+(\w+)\s*=\s*([\w:]+)\s*;', code)
    for alias, full in aliases:
        code = re.sub(r'\b' + re.escape(alias) + r'::', full + '::', code)
    failures = []
    imports = re.findall(r'\busing\s+namespace\s+([\w:]+)\s*;', code)
    for function in operations:
        if not function.startswith('qualpf::'):
            continue
        namespace, short = function.rsplit('::', 1)
        qualified = (re.escape(function) if namespace == 'qualpf' else
                     '(?:qualpf::)?' + re.escape(function.removeprefix('qualpf::')))
        if re.search(r'(?<![\w:])' + qualified + r'\b', code):
            failures.append(f'Core operation referenced in GUI: {name} -> {function}')
        elif namespace in imports or namespace.removeprefix('qualpf::') in imports:
            # Member calls and a locally defined GUI operation are distinct.
            local = re.search(r'(?:\b\w+\s+|::)' + re.escape(short) +
                              r'\s*\([^;{}]*\)\s*(?:const\s*)?(?:\{|;)', code)
            if not local and re.search(r'(?<![\w:.>])' + re.escape(short) + r'\s*\(', code):
                failures.append(f'Imported Core operation in GUI: {name} -> {function}')
    if re.search(r'\b(?:class|struct)\s+\w+\s*:[^{;]*\bSql\w+', code):
        failures.append(f'SQL base class in GUI: {name}')
    return sorted(set(failures))


def concrete_service_failures(code, name):
    tokens = re.sub(r'"(?:\\.|[^"\\])*"', '', code)
    if re.search(r'\b(?:QFile|QSaveFile|QSettings|QProcess|QNetworkAccessManager|QNetworkRequest|QNetworkReply|QJsonDocument|QJsonObject|QSql\w+)\b', tokens):
        return [f'Concrete persistence/process/network service in GUI: {name}']
    variables = re.findall(r'\bQFileInfo\s+(\w+)\s*[({]', code)
    for variable in variables:
        if re.search(r'\b' + re.escape(variable) + r'\.(?:exists|lastModified|size|isFile|isDir|permissions|isReadable)\s*\(', code):
            return [f'Concrete file metadata service in GUI: {name}']
    if re.search(r'\.(?:entryList|entryInfoList|mkpath)\s*\(', code):
        return [f'Concrete directory service in GUI: {name}']
    return []


def source_failures(root, targets):
    failures, dependencies = [], []
    files = [path for base in ('src', 'apps') for path in (root / base).rglob('*')
             if path.suffix in ('.h', '.cpp')]
    index = {}
    for path in files:
        index.setdefault(path.name, []).append(path)
    def visible_dirs(name, seen=None):
        seen = set() if seen is None else seen
        if name in seen or name not in targets:
            return []
        seen.add(name)
        target = targets[name]
        result = [Path(item) for item in parts(target.get('INTERFACE_INCLUDE_DIRECTORIES', ''))]
        for dependency in parts(target.get('INTERFACE_LINK_LIBRARIES', '')):
            result.extend(visible_dirs(dependency, seen))
        return result
    owners, directories = {}, {}
    for name, target in targets.items():
        directory = Path(target.get('SOURCE_DIR', ''))
        includes = [Path(item) for item in parts(target.get('INCLUDE_DIRECTORIES', ''))]
        for dependency in parts(target.get('LINK_LIBRARIES', '')):
            includes.extend(visible_dirs(dependency))
        directories[name] = includes
        for source in parts(target.get('SOURCES', '')):
            owners.setdefault((directory / source).resolve(), []).append(name)
    operations = core_operations(root)
    for path in sorted(files):
        name = relative(path, root)
        origin = layer(name)
        if origin not in ALLOWED:
            continue
        code = strip_comments(path.read_text(encoding='utf-8-sig'))
        owned_by = owners.get(path.resolve(), [])
        if not owned_by:
            # Headers not listed as target sources belong to their deepest
            # configured source directory. Their API dependencies are checked too.
            possible = [name for name, target in targets.items()
                        if relative(path, Path(target.get('SOURCE_DIR', '')))]
            if possible:
                owned_by = [max(possible, key=lambda name: len(targets[name]['SOURCE_DIR']))]
        include_dirs = [item for name in owned_by for item in directories[name]]
        public_api = name.startswith('src/application/include/')
        public_dirs = [item for target in owned_by for item in visible_dirs(target)]
        include_dirs += [root / 'src/application/include', root / 'src/core/include', root / 'src', root]
        if origin in ('Core', 'Application'):
            tokens = re.sub(r'"(?:\\.|[^"\\])*"', '', code)
            if re.search(r'\b(?:QString|QByteArray|QObject|QVariant|QSql\w*|QWidget|QColor|QPen|QSettings)\b', tokens):
                failures.append(f'Qt type in {origin}: {name}')
        for include in INCLUDE.findall(code):
            if origin in ('Core', 'Application') and QT.match(include):
                failures.append(f'Qt header in {origin}: {name} -> {include}')
            candidates = [path.parent / include] + [directory / include for directory in include_dirs]
            resolved = next((item for item in candidates if item.is_file()), None)
            if resolved is None and '/' not in include:
                candidates = index.get(include, [])
                if len(candidates) == 1:
                    resolved = candidates[0]
            if resolved:
                target_name = relative(resolved, root)
                destination = layer(target_name)
                dependencies.append([name, include, target_name])
                if public_api and destination and not any(
                    (directory / include).is_file() for directory in [path.parent] + public_dirs
                ):
                    failures.append(f'Private include needed by public Application API: {name} -> {include}')
                if destination and destination not in ALLOWED[origin]:
                    failures.append(f'Forbidden header owner: {name} -> {target_name} (via {include})')
                if target_name.startswith('tests/'):
                    failures.append(f'Test header in production: {name} -> {target_name}')
        if origin == 'GUI':
            failures.extend(operation_failures(code, name, operations))
            failures.extend(concrete_service_failures(code, name))
    if (root / 'src/compat').exists():
        failures.append('src/compat remains in the production tree')
    return sorted(set(failures)), dependencies, len(operations)


def workflow_failures(root, dependencies):
    """Formal numerics cannot depend, even transitively, on recall diagnostics."""
    failures, edges = [], {}
    for source, _, destination in dependencies:
        edges.setdefault(source, set()).add(destination)
    protected = ('src/core/profile/pfsm_expansion.cpp', 'src/core/profile/pfsm_fitting.cpp',
                 'src/core/profile/pfsm_nnls.cpp', 'src/core/profile/pfsm_trf.cpp',
                 'src/application/session/pfsm_formal_fit.cpp',
                 'src/application/session/pfsm_formal_fit.h')
    for source in protected:
        pending, seen = list(edges.get(source, ())), set()
        while pending:
            destination = pending.pop()
            if destination in seen:
                continue
            seen.add(destination)
            if 'pfsm_candidate_recall' in destination:
                failures.append(f'Formal fitting depends on candidate recall: {source} -> {destination}')
            pending.extend(edges.get(destination, ()))
    for path in (root / 'src/application/session').glob('pfsm_candidate_recall*'):
        if path.suffix in ('.h', '.cpp') and re.search(r'\bPfsmAnalysis\b', strip_comments(path.read_text(encoding='utf-8'))):
            failures.append(f'Candidate recall accesses whole analysis: {relative(path, root)}')
    api = root / 'src/application/include/qualpf/application/pfsm.h'
    if api.exists() and re.search(r'enum class PfsmCandidateRecallVersion\s*\{[^}]*\bv1\b',
                                  strip_comments(api.read_text(encoding='utf-8'))):
        failures.append('Retired v1 candidate recall is executable')
    return failures


def self_test():
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        for name, content in {
            'src/gui/probe.cpp': '#include "sql_hidden.h"\n#include "../core/value.h"\n',
            'src/infrastructure/sql_hidden.h': '', 'src/core/value.h': '',
            'src/core/qt.h': '#include <QtCore/QString>\n',
            'src/application/qt.h': '#include <QObject>\n',
            'src/application/include/qualpf/application/bad_api.h': '#include "core/value.h"\n',
        }.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding='utf-8')
        public_target = {'app': {'SOURCE_DIR': str(root / 'src/application'),
                                'INCLUDE_DIRECTORIES': str(root / 'src'),
                                'INTERFACE_INCLUDE_DIRECTORIES': str(root / 'src/application/include')}}
        errors, _, _ = source_failures(root, public_target)
        assert sum('Forbidden header owner' in item for item in errors) == 2
        assert sum('Qt header' in item for item in errors) == 2
        assert any('Private include needed by public Application API' in item for item in errors)
        operations = {'qualpf::numerics::to_two_theta', 'qualpf::validate'}
        for code in ('qualpf::numerics::to_two_theta(x,y);',
                     'namespace n=qualpf::numerics; n::to_two_theta(x,y);',
                     'using namespace qualpf::numerics; auto x=to_two_theta(a,b);',
                     'using qualpf::numerics::to_two_theta;',
                     'using namespace qualpf; auto x=validate(a);',
                     'struct SearchRequest : public SqlCandidateQuery {};'):
            assert operation_failures(code, 'negative.cpp', operations), code
        for code in ('QSettings settings;', 'QProcess child;', 'QFile file(path);',
                     'QFileInfo info(path); info.lastModified();', 'directory.entryList();'):
            assert concrete_service_failures(code, 'negative.cpp'), code
        assert not operation_failures('using namespace qualpf; sink.validate(frame);', 'positive.cpp', operations)
        for edges in ({'gui': 'infra'}, {'gui': 'core'}, {'gui': 'facade', 'facade': 'infra'},
                      {'app': 'facade', 'facade': 'Qt6::Core'}, {'core': 'Qt6::Core'},
                      {'gui': 'snapshot'}, {'gui': 'app', 'app': 'infra'}):
            directories = {'gui': 'src/gui', 'app': 'src/application', 'core': 'src/core',
                           'infra': 'src/infrastructure', 'snapshot': 'tests/support/snapshot'}
            targets = {key: {'SOURCE_DIR': str(root / directories.get(key, 'external')),
                             'LINK_LIBRARIES': edges.get(key, '')}
                       for key in set(edges) | set(edges.values()) if not key.startswith('Qt6::')}
            assert graph_failures(targets, root), edges
        valid = {key: {'SOURCE_DIR': str(root / directory), 'LINK_LIBRARIES': link}
                 for key, directory, link in [('gui', 'src/gui', 'app'),
                                              ('app', 'src/application', 'core'),
                                              ('core', 'src/core', '')]}
        assert not graph_failures(valid, root)
        valid['app']['INTERFACE_INCLUDE_DIRECTORIES'] = str(root / 'src')
        assert graph_failures(valid, root)
        formal = 'src/application/session/pfsm_formal_fit.cpp'
        recall = 'src/core/include/qualpf/profile/pfsm_candidate_recall.h'
        assert workflow_failures(root, [[formal, 'bridge.h', 'bridge.h'], ['bridge.h', recall, recall]])
        assert not workflow_failures(root, [[formal, 'pfsm.h', 'src/core/include/qualpf/profile/pfsm.h']])
        path = root / 'src/application/session/pfsm_candidate_recall.cpp'
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('void recall(PfsmAnalysis &state);', encoding='utf-8')
        assert workflow_failures(root, [])
    print('PASS: header, operation, SQL inheritance, transitive link and include-export negative controls')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--graph', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--report-only', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
    targets = json.loads(args.graph.read_text(encoding='utf-8-sig'))['targets']
    failures, headers, operations = source_failures(ROOT, targets)
    failures += workflow_failures(ROOT, headers)
    failures += graph_failures(targets, ROOT)
    failures += observe()['failures']
    result = dict(schema='qualpf.strict-boundaries.v1', status='FAIL' if failures else 'PASS',
                  failures=sorted(set(failures)), resolved_headers=headers,
                  indexed_core_operations=operations, configured_targets=len(targets))
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f"{result['status']}: strict boundaries; {len(targets)} configured targets, {operations} Core operations")
    for failure in result['failures']:
        print(failure)
    return bool(failures) and not args.report_only


if __name__ == '__main__':
    raise SystemExit(main())
