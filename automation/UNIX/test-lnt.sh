# Test clang on UNIX using Visual Studio Team Services

set -ue
set -o pipefail
set -x

if [ -z "$LNT" ]; then
  exit 0;
fi

CLANG=$(LLVM_OBJ_DIR)/$(BUILDCONFIGURATION)/bin/clang

if [ ! -a "$CLANG" ]; then
  echo "clang compiler not found at %CLANG%"
  exit 1
fi

"$LNT_SCRIPT" runtest nt --sandbox "$LNT_RESULTS" -cc "$CLANG" --test-suite ${BUILD_SOURCESDIRECTORY}/llvm-test-suite
    --cflags -fcheckedc-extensions

set +ue
set +o pipefail


