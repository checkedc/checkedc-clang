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
  --no-timestamp \
  --cc "$CC" \
  --cxx "$CXX" \
  --cflags "$CFLAGS" \
  --test-suite ${BUILD_SOURCESDIRECTORY}/llvm-test-suite \
  --make-param=ExtraHeaders=${BUILD_SOURCESDIRECTORY}/llvm/projects/checkedc-wrapper/checkedc/include \
  --output=${RESULT_DATA} \
  -j${BUILD_CPU_COUNT} \
  | tee ${RESULT_SUMMARY}

set +ue
set +o pipefail
