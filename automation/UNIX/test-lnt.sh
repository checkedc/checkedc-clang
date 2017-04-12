# Test clang on UNIX using Visual Studio Team Services

set -ue
set -o pipefail
set -x

if [ -z "$LNT" ]; then
  exit 0;
fi

CLANG=$(LLVM_OBJ_DIR)/$(BUILDCONFIGURATION)/bin/clang
RESULT_LOG="${LNT_RESULTS_DIRS}/result.log"

if [ ! -a "$CLANG" ]; then
  echo "clang compiler not found at %CLANG%"
  exit 1
fi

"$LNT_SCRIPT" runtest nt --sandbox "$LNT_RESULTS_DIR" --notimestamp
   --cc "$CLANG" --test-suite ${BUILD_SOURCESDIRECTORY}/llvm-test-suite \
   --cflags -fcheckedc-extension \
   --output="${LNT_RESULTS_DIR}/result.log -j${$BUILD_CPU_COUNT}

cat ${RESULT_LOG}

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
