#!/bin/bash
set -euo pipefail
TASK_ACTION=${1:-build}
TASK_BUILD=${2:?fresh build directory}
TASK_RAW=${3:?raw output directory}
TASK_SOURCE=$(cd "$(dirname "$0")/../.." && pwd)
export MKL_NUM_THREADS=1 QT_QPA_PLATFORM=offscreen
mkdir -p "$TASK_RAW" "$TASK_BUILD/tmp"
export TMPDIR="$TASK_BUILD/tmp"
if [ "$TASK_ACTION" = build ]; then
    test ! -f "$TASK_BUILD/CMakeCache.txt"
    test ! -d "$TASK_BUILD/CMakeFiles"
    python3 - "$TASK_RAW" "$TASK_BUILD" "$TASK_SOURCE" <<'PY'
import datetime,json,sys
from pathlib import Path
Path(sys.argv[1], 'linux-gnu-fresh-start.json').write_text(json.dumps(dict(
 id='linux-gnu', build=sys.argv[2], cache_existed_before=False, objects_existed_before=False,
 utc=datetime.datetime.now(datetime.timezone.utc).isoformat()),indent=2)+'\n')
Path(sys.argv[1], 'linux-gnu-commands-used.json').write_text(json.dumps(dict(
 configure=['cmake','-S',sys.argv[3],'-B',sys.argv[2],'-G','Ninja','-DCMAKE_BUILD_TYPE=RELEASE',
 '-DQUALPF_BUILD_PRODUCT_TESTS=ON','-DQUALPF_BUILD_PREPROCESSING_TESTS=ON','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'],
 build=['cmake','--build',sys.argv[2],'--parallel','2'],
 test=['ctest','--test-dir',sys.argv[2],'-L','normal','--output-on-failure']),indent=2)+'\n')
PY
    mkdir -p "$TASK_BUILD/.cmake/api/v1/query"
    touch "$TASK_BUILD/.cmake/api/v1/query/codemodel-v2"
    cmake -S "$TASK_SOURCE" -B "$TASK_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RELEASE \
        -DQUALPF_BUILD_PRODUCT_TESTS=ON -DQUALPF_BUILD_PREPROCESSING_TESTS=ON \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >"$TASK_RAW/linux-gnu-configure.log" 2>&1
    cmake --build "$TASK_BUILD" --parallel 2 >"$TASK_RAW/linux-gnu-build.log" 2>&1
    TASK_PURE="$TASK_BUILD/qtfree-application"
    cmake -S "$TASK_SOURCE/tests/architecture" -B "$TASK_PURE" -G Ninja \
        -DCMAKE_BUILD_TYPE=RELEASE >"$TASK_RAW/linux-gnu-qtfree-configure.log" 2>&1
    cmake --build "$TASK_PURE" --parallel 2 >"$TASK_RAW/linux-gnu-qtfree-build.log" 2>&1
    "$TASK_PURE/application_boundary" >"$TASK_RAW/linux-gnu-qtfree-test.log" 2>&1
    tail -n 4 "$TASK_RAW/linux-gnu-build.log"
    cat "$TASK_RAW/linux-gnu-qtfree-test.log"
elif [ "$TASK_ACTION" = test ]; then
    ctest --test-dir "$TASK_BUILD" -L normal --output-on-failure \
        --output-junit "$TASK_RAW/linux-gnu-tests.xml" >"$TASK_RAW/linux-gnu-test.log" 2>&1
    python3 -B "$TASK_SOURCE/tools/architecture/capture_build.py" \
        --build "$TASK_BUILD" --id linux-gnu --raw "$TASK_RAW" >"$TASK_RAW/linux-gnu-capture.log" 2>&1
    python3 -B "$TASK_SOURCE/tools/validation/check_zero_fortran.py" \
        --build "$TASK_BUILD" --objdump /usr/bin/objdump --nm /usr/bin/nm \
        --output "$TASK_RAW/linux-gnu-zero-fortran.json" >"$TASK_RAW/linux-gnu-zero-fortran.log" 2>&1
    tail -n 10 "$TASK_RAW/linux-gnu-test.log"
else
    echo "Unknown action: $TASK_ACTION" >&2
    exit 2
fi
