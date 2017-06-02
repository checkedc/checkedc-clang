# Test clang on UNIX using Visual Studio Team Services

set -ue
set -o pipefail
set -x

if [ -z "$LNT" ]; then
  exit 0;
fi

CLANG=${LLVM_OBJ_DIR}/bin/clang
RESULT_DATA="${LNT_RESULTS_DIR}/data.xml"
RESULT_SUMMARY="${LNT_RESULTS_DIR}/result.log"


if [ ! -e "$CLANG" ]; then
  echo "clang compiler not found at $CLANG"
  exit 1
fi

"$LNT_SCRIPT" runtest nt --sandbox "$LNT_RESULTS_DIR" --no-timestamp \
   --cc "$CLANG" --test-suite ${BUILD_SOURCESDIRECTORY}/llvm-test-suite \
   --cflags -fcheckedc-extension \
   --make-param=ExtraHeaders=${BUILD_SOURCESDIRECTORY}/llvm/projects/checkedc-wrapper/checkedc/include \
   -v --output=${RESULT_DATA} -j${BUILD_CPU_COUNT} | tee ${RESULT_SUMMARY}

# Code for running tests using cmake.  Needs further testing before enabling.
#
# "$LNT_SCRIPT" runtest test-suite --sandbox "$LNT_RESULTS_DIR" --no-timestamp \
# --cc "$CLANG" --test-suite ${BUILD_SOURCESDIRECTORY}/llvm-test-suite \
#   --cflags -fcheckedc-extension \
#  --use-cmake=/usr/local/bin/cmake \
#   --cmake-cache ${BUILDCONFIGURATION} \
#   --use-lit=${BUILD_SOURCESDIRECTORY}/llvm/utilkkvs/lit/lit.py \
#   -v | tee ${RESULT_SUMMARY}

if grep FAIL ${RESULT_SUMMARY}; then
  echo "LNT testing failed."
  exit 1
else
  if [ $? -eq 2 ]; then
    echo "Grep of LNT result log unexpectedly failed."
    exit 1
  fi
fi

set +ue
set +o pipefail
