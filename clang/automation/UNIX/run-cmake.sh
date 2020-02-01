#!/usr/bin/env bash
# 
# Run CMake to create build system in $LLVM_OBJ_DIR

set -ue
set -o pipefail
set -x

if [[ ("$LNT" != "" || "$BUILD_PACKAGE" == "Yes") && ("$BUILDCONFIGURATION" == "Release" || "$BUILDCONFIGURATION" == "ReleaseWithDebInfo") ]]; then
  CMAKE_ADDITIONAL_OPTIONS="-DLLVM_INSTALL_TOOLCHAIN_ONLY=ON -DLLVM_ENABLE_ASSERTIONS=ON"
else
  CMAKE_ADDITIONAL_OPTIONS=""
fi

(cd "$LLVM_OBJ_DIR";
 cmake -G "Unix Makefiles" \
   ${CMAKE_ADDITIONAL_OPTIONS} \
  -DLLVM_ENABLE_PROJECTS=clang \
  -DCMAKE_BUILD_TYPE="$BUILDCONFIGURATION" \
  -DLLVM_LIT_ARGS="-sv --no-progress-bar" \
  "$BUILD_SOURCESDIRECTORY/checkedc-clang/llvm")

set +ue
set +o pipefail
