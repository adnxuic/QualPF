#!/usr/bin/env python3
"""Capture actual build/test inputs and artifacts without inferring test success."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from check_boundaries import ROOT, sha, source_manifest

def capture(build, identifier, raw):
    marker = json.loads((raw / (identifier + '-fresh-start.json')).read_bytes())
    if marker.get('cache_existed_before') is not False or marker.get('objects_existed_before') is not False:
        raise ValueError('Fresh observation requires an empty-cache/object start marker')
    cache = (build / 'CMakeCache.txt').read_text(encoding='utf-8-sig')
    compiler_file = next((build / 'CMakeFiles').glob('*/CMakeCXXCompiler.cmake'))
    compiler = compiler_file.read_text(encoding='utf-8-sig')
    def value(key):
        match = re.search(r'^' + re.escape(key) + r':[^=]+=(.*)$', cache, re.M)
        return match[1].strip() if match else ''
    def compiler_value(key):
        match = re.search(r'set\(' + re.escape(key) + r' "([^\"]+)"\)', compiler)
        return match[1] if match else ''
    commands = json.loads((build / 'compile_commands.json').read_bytes())
    pure_commands = [item for item in commands if '/src/core/' in item['file'].replace('\\', '/') or
                     '/src/application/' in item['file'].replace('\\', '/')]
    qt_contamination = [item['file'] for item in pure_commands if
                        re.search(r'QT_(?:CORE|GUI|WIDGETS|SQL)_LIB|[\\/]Qt(?:Core|Gui|Widgets|Sql)[\\/]',
                                  item.get('command', ' '.join(item.get('arguments', []))))]
    reply = build / '.cmake/api/v1/reply'
    index_file = max(reply.glob('index-*.json'), key=lambda path: path.stat().st_mtime_ns)
    index = json.loads(index_file.read_bytes())
    model = json.loads((reply / index['reply']['codemodel-v2']['jsonFile']).read_bytes())
    targets = {target['name']: json.loads((reply / target['jsonFile']).read_bytes())
               for target in model['configurations'][0]['targets']}
    target_names = {target['id']: name for name, target in targets.items()}
    target_edges = {name: [target_names[dependency['id']] for dependency in target.get('dependencies', [])
                           if dependency['id'] in target_names] for name, target in targets.items()}
    fragments = {name: [' '.join(fragment['fragment'] for fragment in group.get('compileCommandFragments', []))
                        for group in target.get('compileGroups', [])]
                 for name, target in targets.items() if name in ('qualpf_core', 'qualpf_application', 'qualpf_session')}
    artifacts = {}
    for path in list(build.glob('*.exe')) + list(build.glob('src/*.exe')) + list(build.glob('qualpf*')):
        if not path.is_file():
            continue
        if path.suffix == '.exe' or path.read_bytes()[:4] == b'\x7fELF':
            artifacts[path.relative_to(build).as_posix()] = sha(path)
    product = build / 'src/qualpf.exe' if identifier.startswith('windows') else build / 'src/qualpf'
    if product.is_file():
        artifacts[product.relative_to(build).as_posix()] = sha(product)
    raw.mkdir(parents=True, exist_ok=True)
    (raw / (identifier + '-cache.txt')).write_bytes((build / 'CMakeCache.txt').read_bytes())
    (raw / (identifier + '-compiler.cmake')).write_bytes(compiler_file.read_bytes())
    (raw / (identifier + '-commands.json')).write_bytes((build / 'compile_commands.json').read_bytes())
    (raw / (identifier + '-targets.json')).write_text(json.dumps(targets, indent=2) + '\n', encoding='utf-8')
    ctest = value('CMAKE_CTEST_COMMAND') or 'ctest'
    listing = subprocess.check_output([ctest, '--test-dir', str(build), '-L', 'normal', '--show-only=json-v1'],
                                      text=True, encoding='utf-8')
    (raw / (identifier + '-normal-list.json')).write_text(listing, encoding='utf-8')
    actual_inputs = source_manifest()
    expected_inputs = json.loads((raw / 'source-inputs.json').read_bytes())
    if actual_inputs != expected_inputs:
        raise ValueError('Source/test/configuration inputs changed during fresh build validation')
    dependency_count = None
    if identifier == 'windows-msvc':
        ninja = value('CMAKE_MAKE_PROGRAM')
        deps = subprocess.check_output([ninja, '-C', str(build), '-t', 'deps'], text=True, encoding='utf-8', errors='replace')
        (raw / (identifier + '-ninja-deps.txt')).write_text(deps, encoding='utf-8')
        match = re.search(r'qualpf_gui.dir[\\/]mainwindow.cpp.obj: #deps ([0-9]+)', deps)
        dependency_count = int(match[1]) if match else 0
    result = dict(schema='qualpf.phase37.build-observation.v1', id=identifier,
                  validation_kind='FRESH_VALIDATION', build_directory=str(build), build_type=value('CMAKE_BUILD_TYPE'),
                  compiler_id=compiler_value('CMAKE_CXX_COMPILER_ID'),
                  compiler_version=compiler_value('CMAKE_CXX_COMPILER_VERSION'),
                  qt_directory=value('Qt6_DIR'), product_tests=value('QUALPF_BUILD_PRODUCT_TESTS'),
                  preprocessing_tests=value('QUALPF_BUILD_PREPROCESSING_TESTS'),
                  compiler_fingerprint=sha(compiler_file), commands_fingerprint=sha(build / 'compile_commands.json'),
                  cache_fingerprint=sha(build / 'CMakeCache.txt'), source_sha256=actual_inputs,
                  qt_contamination_in_pure_commands=qt_contamination, pure_compile_units=len(pure_commands),
                  target_edges=target_edges, numerical_compile_fragments=fragments,
                  msvc_mainwindow_header_dependencies=dependency_count, artifacts_sha256=artifacts)
    (raw / (identifier + '-fingerprint.json')).write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('Captured ' + identifier + ': ' + str(len(commands)) + ' actual compile units, ' +
          str(len(artifacts)) + ' executable artifacts')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path)
    parser.add_argument('--id')
    parser.add_argument('--raw', type=Path, required=True)
    parser.add_argument('--freeze-inputs', action='store_true')
    args = parser.parse_args()
    if args.freeze_inputs:
        args.raw.mkdir(parents=True, exist_ok=True)
        (args.raw / 'source-inputs.json').write_text(json.dumps(source_manifest(), indent=2) + '\n', encoding='utf-8')
    else:
        capture(args.build.resolve(), args.id, args.raw.resolve())

if __name__ == '__main__':
    main()
