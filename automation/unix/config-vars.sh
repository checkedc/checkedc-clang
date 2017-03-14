#!/usr/bin/env bash
# 
# Validate and set configuration variables.   Other scripts should only 
# depend on variables printed at the end of this script.
#
# This script is run as part of automated build and test validation.  
# It has extra checking so that it can be run manually as well. It validates
# that environment variables set by the system have been are present. When
# running it manually, the variables must be set by the user.

set -e
set -o pipefail

# Create configuration variables

# Validate build configuration

if [ -z "$BUILDCONFIGURATION" ]; then
  echo "BUILDCONFIGURATION not set: must be set to set to one of Debug, Release, ReleaseWithDebInfo"
  exit 1
elif [ "$BUILDCONFIGURATION" != "Debug" -a "$BUILDCONFIGURATION" != "Release" -a \
       "$BUILDCONFIGURATION" != "ReleaseWithDebInfo" ]; then
  echo "Unknown BUILDCONFIGURATION value $BUILDCONFIGURATION: must be one of Debug, Release, ReleaseWithDebInfo"
  exit 1
fi

# Validate build OS

if [ -z "$BUILDOS" ]; then
  export BUILDOS="Linux"
elif [ "$BUILDOS" != "Linux" -a "$BUILDOS" != "WSL" ]; then
  echo "Unknown BUILDOS value $BUILDOS: must be Linux or WSL"
  exit 1
fi

# Validate or set target architecture for testing.

if [ -z "$TEST_TARGET_ARCH" ]; then
  export TEST_TARGET_ARCH="X86"
elif [ "$TEST_TARGET_ARCH" != "X86" -a "$TEST_TARGET_ARCH" != "AMD64" ]; then
  echo "Unknown TEST_TARGET_ARCH value $TEST_TARGET_ARCH: must be X86 or AMD64"
  exit 1
fi

if [ -z $BUILD_BINARIESDIRECTORY ]; then
  echo "BUILD_BINARIESDIRECTORY not set.  Set it the directory that will contain the object directory."
  exit 1
fi

if [ -z $BUILD_SOURCESDIRECTORY ]; then
   echo "BUILD_SOURCESDIRECTORY not set.  Set it the directory that will contain the sources directory."
   exit 1
fi

export LLVM_OBJ_DIR="${BUILD_BINARIESDIRECTORY}/LLVM-${BUILDCONFIGURATION}-${TEST_TARGET_ARCH}-${BUILDOS}.obj"

# Validate Test Suite configuration

if [ -z "$TEST_SUITE" ]; then
  echo "TEST_SUITE not set: must be set to one of CheckedC, CheckedC_clang, or CheckedC_LLVM"
  exit 1
elif [ "$TEST_SUITE" != "CheckedC" -a "$TEST_SUITE" != "CheckedC_clang" -a \
       "$TEST_SUITE" != "CheckedC_LLVM" ]; then
  echo "Unknown TEST_SUITE value $TEST_SUITE: must be one of CheckedC, CheckedC_clang, or CheckedC_LLVM"
  exit 1
fi

# set up branch names
if [ -z "$LLVM_BRANCH" ]; then
  export LLVM_BRANCH="master"
fi

if [ -z "$CHECKEDC_BRANCH" ]; then
  export CHECKEDC_BRANCH="master"
fi

if [ -z "$CLANG_BRANCH" ]; then
  export CLANG_BRANCH="master"
fi

# set up source versions (Git commit number)
if [ -z "$LLVM_COMMIT" ]; then
  export LLVM_COMMIT="HEAD"
fi

if [ -z "$CHECKEDC_COMMIT" ]; then
 export CHECKEDC_COMMIT="HEAD"
fi

if [ -z "$CLANG_COMMIT" ]; then
  export CLANG_COMMIT="HEAD"
fi

if [ -z "$BUILD_CPU_COUNT" ]; then
  declare -i NPROC=$(nproc);
  export BUILD_CPU_COUNT=$(($NPROC/2))
fi

echo "Configured environment variables:"
echo
echo " BUILDCONFIGURATION: $BUILDCONFIGURATION"
echo " BUILDOS: $BUILDOS"
echo " TEST_TARGET_ARCH: $TEST_TARGET_ARCH"
echo " TEST_SUITE: $TEST_SUITE"
echo
echo " Directories:"
echo "  BUILD_SOURCESDIRECTORY: $BUILD_SOURCESDIRECTORY"
echo "  LLVM_OBJ_DIR: $LLVM_OBJ_DIR"
echo 
echo " Branch and commit information:"
echo "  CLANG_BRANCH: $CLANG_BRANCH"
echo "  CLANG_COMMIT: $CLANG_COMMIT"
echo "  LLVM_BRANCH: $LLVM_BRANCH"
echo "  LLVM_COMMIT: $LLVM_COMMIT"
echo "  CHECKEDC BRANCH: $CHECKEDC_BRANCH"
echo "  CHECKEDC_COMMIT: $CHECKEDC_COMMIT"
echo
echo " BUILD_CPU_COUNT: $BUILD_CPU_COUNT"

exit 0
