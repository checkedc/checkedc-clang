# Test clang on UNIX using Visual Studio Team Services

set -ue
set -o pipefail
set -x

if [ -z "$LNT" ]; then
  exit 0;
fi

CLANG=${LLVM_OBJ_DIR}/bin/clang
RESULT_LOG="${LNT_RESULTS_DIR}/result.log"


if [ ! -e "$CLANG" ]; then
  echo "clang compiler not found at $CLANG"
  exit 1
fi

"$LNT_SCRIPT" runtest nt --sandbox "$LNT_RESULTS_DIR" --no-timestamp \
   --cc "$CLANG" --test-suite ${BUILD_SOURCESDIRECTORY}/llvm-test-suite \
   --cflags -fcheckedc-extension \
   -v --output=${RESULT_LOG} -j${BUILD_CPU_COUNT}

if grep FAILED ${RESULT_LOG}; then
  echo "LNT testing failed."
  exit 1
else
  if [ $? -eq 2 ]; then
    echo "Grep of LNT result log unexepectedly failed."
    exit 1
  fi
fi

set +ue
set +o pipefail
