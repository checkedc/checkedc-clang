#!/usr/bin/env bash
# 
# Run CMake to create build system in $LLVM_OBJ_DIR

set -ue
set -o pipefail
set -x

if [ "${TEST_TARGET_ARCH}" == "X86" ]; then
  CMAKE_ADDITIONAL_OPTIONS="-DLLVM_TARGET_ARCH=X86"
else
  CMAKE_ADDITIONAL_OPTIONS=""
fi

if [[ "$LNT" != "" && ("$BUILDCONFIGURATION" == "Release" || "$BUILDCONFIGURATION" == "ReleaseWithDebInfo") ]]; then
  CMAKE_ADDITIONAL_OPTIONS="$CMAKE_ADDITIONAL_OPTIONS -DLLVM_ENABLE_ASSERTIONS=On"
fi

(cd "$LLVM_OBJ_DIR";
 cmake -G "Unix Makefiles" \
   ${CMAKE_ADDITIONAL_OPTIONS} -DCMAKE_BUILD_TYPE="$BUILDCONFIGURATION" \
  -DLLVM_LIT_ARGS="-sv --no-progress-bar" \
  "$BUILD_SOURCESDIRECTORY/llvm")

set +ue
set +o pipefail
