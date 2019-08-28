# Test clang on UNIX using Visual Studio Team Services.

set -ue
set -o pipefail
set -x

CC=clang
CXX=${CC}++
CFLAGS="-fcheckedc-extension"

"$LNT_SCRIPT" runtest nt \
  -v \
  --sandbox "$RESULTS_DIR" \
  --cc "$CC" \
  --cxx "$CXX" \
  --cflags "$CFLAGS" \
  --test-suite ${BUILD_SOURCESDIRECTORY}/llvm-test-suite \
  --output=${RESULT_DATA} \
  -j${BUILD_CPU_COUNT} \
  2>&1 | tee ${RESULT_SUMMARY}

set +ue
set +o pipefail
