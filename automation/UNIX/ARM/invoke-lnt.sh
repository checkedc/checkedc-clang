# Test clang on UNIX using Visual Studio Team Services.

set -ue
set -o pipefail
set -x

CURDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

CC=$CURDIR/clang-arm-x
CXX=${CC}++
CFLAGS="-fcheckedc-extension -static"
RUN=qemu-arm

"$LNT_SCRIPT" runtest nt \
  -v \
  --sandbox "$RESULTS_DIR" \
  --cc "$CC" \
  --cxx "$CXX" \
  --cflags "$CFLAGS" \
  --qemu-user-mode "$RUN" \
  --test-suite ${BUILD_SOURCESDIRECTORY}/llvm-test-suite \
  --output=${RESULT_DATA} \
  -j${BUILD_CPU_COUNT} \
  2>&1 | tee ${RESULT_SUMMARY}

set +ue
set +o pipefail
