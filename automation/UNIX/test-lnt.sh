# Test clang on UNIX using Visual Studio Team Services

set -ue
set -o pipefail
set -x

if [ -z "$LNT" ]; then
  exit 0;
fi

CLANG=${LLVM_OBJ_DIR}/bin/clang

if [ ! -e "$CLANG" ]; then
  echo "clang compiler not found at $CLANG"
  exit 1
fi

"$LNT_SCRIPT" runtest nt --sandbox "${LNT_RESULTS_DIR}" --cc "$CLANG" \
	      --test-suite ${BUILD_SOURCESDIRECTORY}/llvm-test-suite \
	      --no-timestamp --cflags -fcheckedc-extension -j${BUILD_CPU_COUNT}

# TODO: test that no failures occurred during testing.

set +ue
set +o pipefail


