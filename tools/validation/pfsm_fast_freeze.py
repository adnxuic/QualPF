#!/usr/bin/env python3
"""Freeze actual PFSM source, build configuration and an already verified runtime."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'build', 'runtime', 'output'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--revision', required=True)
    p.add_argument('--additional-executable', type=Path, action='append', default=[])
    p.add_argument('--repository', type=Path, required=True)
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    archive = subprocess.check_output(['git', '-C', str(args.repository), 'archive', '--format=tar', args.revision])
    reference = tarfile.open(fileobj=io.BytesIO(archive), mode='r:')
    tracked = [item.name for item in reference.getmembers() if item.isfile()]
    source_files = {}
    checkout_line_endings = []
    for name in tracked:
        path = args.source/name
        target = args.output/'source'/name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
        # The actual compiler input is retained, including checkout line endings.
        source_files[name] = digest(path)
        expected = reference.extractfile(name).read()
        actual = path.read_bytes()
        if actual != expected:
            if b'\0' not in actual and actual.replace(b'\r\n', b'\n') == expected.replace(b'\r\n', b'\n'):
                checkout_line_endings.append(name)
            else:
                raise RuntimeError('Source differs from declared revision: '+name)
    (args.output/'committed-source.tar').write_bytes(archive)
    runtime_binding = json.loads((args.runtime/'binding.json').read_text(encoding='utf-8'))
    runtime_sources = {}
    for path in args.runtime.rglob('*.dll'):
        name = path.relative_to(args.runtime).as_posix()
        if digest(path) != runtime_binding['files'].get(name):
            raise RuntimeError('Unverified runtime '+name)
        target = args.output/name; target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target); runtime_sources[name] = digest(path)
    if not runtime_sources:
        raise RuntimeError('No frozen runtime dependencies')
    executable = args.build/'bin/qualpf_pfsm_benchmark.exe'
    shutil.copyfile(executable, args.output/executable.name)
    for extra in args.additional_executable:
        if extra.suffix.lower() != '.exe' or not extra.is_file() or (args.output/extra.name).exists():
            raise RuntimeError('Invalid/duplicate additional executable: '+str(extra))
        shutil.copyfile(extra, args.output/extra.name)
    for name in ('CMakeCache.txt', 'compile_commands.json', 'build.ninja', 'strict-targets.json'):
        path = args.build/name
        if path.exists():
            shutil.copyfile(path, args.output/name)
    (args.output/'qt.conf').write_text('[Paths]\nPlugins=plugins\n', encoding='utf-8')
    files = {p.relative_to(args.output).as_posix(): digest(p) for p in args.output.rglob('*') if p.is_file()}
    result = dict(schema='qualpf.pfsm.fast-freeze.v1', revision=args.revision,
                  source=str(args.source.resolve()), build=str(args.build.resolve()),
                  runtime_source=str(args.runtime.resolve()), runtime_files=runtime_sources,
                  line_ending_only_checkout_differences=checkout_line_endings,
                  source_files=source_files, files=files)
    (args.output/'binding.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print('Frozen '+args.revision+': '+str(len(source_files))+' sources; '+digest(args.output/executable.name))


if __name__ == '__main__':
    main()
