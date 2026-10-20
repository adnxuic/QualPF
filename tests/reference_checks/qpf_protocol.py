"""Independent SQLite/CBOR/IEEE protocol controls, without Qt or third-party modules."""
import argparse
import hashlib
from pathlib import Path
import sqlite3
import struct
import subprocess


class Undefined:
    pass


UNDEFINED = Undefined()


def encode(value):
    def head(major, n):
        for limit, tag, fmt in ((24, None, None), (256, 24, '>B'),
                                (65536, 25, '>H'), (2**32, 26, '>I'),
                                (2**64, 27, '>Q')):
            if n < limit:
                return bytes([(major << 5) | n]) if tag is None else bytes([(major << 5) | tag]) + struct.pack(fmt, n)
        raise ValueError('integer overflow')
    if value is UNDEFINED:
        return b'\xf7'
    if value is None:
        return b'\xf6'
    if isinstance(value, bool):
        return b'\xf5' if value else b'\xf4'
    if isinstance(value, int):
        return head(0, value) if value >= 0 else head(1, -value - 1)
    if isinstance(value, float):
        return b'\xfb' + struct.pack('>d', value)
    if isinstance(value, str):
        text = value.encode('utf-8')
        return head(3, len(text)) + text
    if isinstance(value, bytes):
        return head(2, len(value)) + value
    if isinstance(value, list):
        return head(4, len(value)) + b''.join(map(encode, value))
    if isinstance(value, dict):
        return head(5, len(value)) + b''.join(encode(k) + encode(value[k]) for k in sorted(value))
    raise ValueError(type(value))


def decode(data):
    offset = 0
    def read(depth=0):
        nonlocal offset
        assert depth <= 64 and offset < len(data)
        first = data[offset]
        offset += 1
        major, info = first >> 5, first & 31
        if major == 7:
            if info == 27:
                value = struct.unpack_from('>d', data, offset)[0]
                offset += 8
                return value
            return {20: False, 21: True, 22: None, 23: UNDEFINED}[info]
        if info < 24:
            n = info
        else:
            fmt = {24: '>B', 25: '>H', 26: '>I', 27: '>Q'}[info]
            n = struct.unpack_from(fmt, data, offset)[0]
            offset += struct.calcsize(fmt)
        if major == 0:
            return n
        if major == 1:
            return -n-1
        if major in (2, 3):
            value = data[offset:offset+n]
            assert len(value) == n
            offset += n
            return value.decode('utf-8') if major == 3 else value
        if major == 4:
            return [read(depth+1) for _ in range(n)]
        if major == 5:
            result = {}
            for _ in range(n):
                key = read(depth+1)
                assert isinstance(key, str) and key not in result
                result[key] = read(depth+1)
            return result
        raise ValueError(major)
    value = read()
    assert offset == len(data)
    return value


def digest(data):
    return hashlib.sha256(data).hexdigest()


def inspect(path, extension=False):
    with sqlite3.connect(f'{Path(path).resolve().as_uri()}?mode=ro', uri=True) as db:
        assert db.execute('pragma application_id').fetchone()[0] == 0x51504650
        assert db.execute('pragma user_version').fetchone()[0] == 1
        assert db.execute('pragma quick_check').fetchone()[0] == 'ok'
        assert {r[0] for r in db.execute("select name from sqlite_master where type='table'")} == {'manifest', 'objects', 'blocks'}
        payload, checksum = db.execute('select payload,sha256 from manifest where id=1').fetchone()
        assert digest(payload) == checksum
        manifest = decode(payload)
        assert manifest['format'] == 'qualpf.project' and manifest['major'] == 1
        objects = {}
        for identity, kind, version, payload, checksum in db.execute('select * from objects'):
            value = decode(payload)
            assert digest(encode([identity, kind, version, value])) == checksum
            objects[identity] = (kind, value)
        blocks = {}
        for checksum, length, payload in db.execute('select * from blocks'):
            assert 0 < length <= 4*1024*1024 and len(payload) == length and digest(payload) == checksum
            blocks[checksum] = payload
        def resolve(ref, kind):
            found_kind, value = objects[ref['$ref']]
            assert found_kind == kind
            return value
        root = resolve(manifest['root'], 'project')
        state = resolve(root['analysis'], 'product_state')
        pattern = resolve(state['pattern'], 'pattern')
        assert pattern['id'] == 2**64-1
        array = pattern['observed']['$array']
        assert array['dtype'] == 'f64' and array['shape'] == [3] and array['byte_order'] == 'little'
        raw = b''.join(blocks[key] for key in array['blocks'])
        assert raw == struct.pack('<ddd', -0., .25, 65536.125)
        metadata = state['project_metadata']['$metadata']
        assert metadata['wide'] == 9007199254740993 and type(metadata['integer']) is int
        assert metadata['unsigned_max'] == 2**64-1 and type(metadata['unsigned_max']) is int
        assert type(metadata['floating']) is float and metadata['undefined'] is UNDEFINED and metadata['null'] is None
        if extension:
            extra = resolve(manifest['independent_extension'], 'independent_optional')
            assert extra['wide'] == 2**64-1 and extra['missing'] is UNDEFINED
        return manifest


def rewrite(source, target):
    inspect(source)
    Path(target).unlink(missing_ok=True)
    with sqlite3.connect(source) as src, sqlite3.connect(target) as dst:
        src.backup(dst)
        manifest = decode(dst.execute('select payload from manifest').fetchone()[0])
        identity = digest(b'independent-python-protocol-object')
        fields = {'wide': 2**64-1, 'missing': UNDEFINED, 'null': None, 'negative_zero': -0.}
        dst.execute('insert into objects values(?,?,?,?,?)', (identity, 'independent_optional', 1, encode(fields), digest(encode([identity, 'independent_optional', 1, fields]))))
        manifest['independent_extension'] = {'$ref': identity}
        manifest['minor'] = 9
        payload = encode(manifest)
        dst.execute('update manifest set payload=?,sha256=?', (payload, digest(payload)))
    inspect(target, extension=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runner')
    parser.add_argument('--directory', type=Path)
    parser.add_argument('--input', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.runner:
        args.directory.mkdir(parents=True, exist_ok=True)
        source, independent, resaved = [args.directory / name for name in ('native.qpf', 'independent.qpf', 'resaved.qpf')]
        subprocess.run([args.runner, '--write-fixture', source], check=True)
        rewrite(source, independent)
        subprocess.run([args.runner, '--resave-fixture', independent, resaved], check=True)
        inspect(resaved, extension=True)
    else:
        rewrite(args.input, args.output)
    print('PASS: independent SQLite, CBOR, IEEE precision and optional-object preservation')


if __name__ == '__main__':
    main()
