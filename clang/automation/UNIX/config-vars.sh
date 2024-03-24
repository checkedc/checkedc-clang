#!/usr/bin/env bash
# 
# Validate and set configuration variables. Other scripts should only
# depend on variables printed at the end of this script.
#
# This script is run as part of automated build and test validation.
# It has extra checking so that it can be run manually as well. It validates
# that environment variables set by the system are present. When running it
# manually, the variables must be set by the user.

# This script has to be run in the context of the parent bash environment
# to export environment variables, so set a status result instead of exiting
# if something goes wrong.

CHECKEDC_CONFIG_STATUS="passed"

# Create configuration variables

# Validate build configuration

if [ -z "$BUILDCONFIGURATION" ]; then
  echo "BUILDCONFIGURATION not set: must be set to set to one of Debug, Release, ReleaseWithDebInfo"
  CHECKEDC_CONFIG_STATUS="error"  
elif [ "$BUILDCONFIGURATION" != "Debug" -a "$BUILDCONFIGURATION" != "Release" -a \
       "$BUILDCONFIGURATION" != "ReleaseWithDebInfo" ]; then
  echo "Unknown BUILDCONFIGURATION value $BUILDCONFIGURATION: must be one of Debug, Release, ReleaseWithDebInfo"
  CHECKEDC_CONFIG_STATUS="error" 
fi

if [ -z "$BUILD_PACKAGE" ]; then
  export BUILD_PACKAGE="No"
elif [ "$BUILD_PACKAGE" != "Yes" -a "$BUILD_PACKAGE" != "No" ]; then
  echo "Unknown BUILD_PACKAGE value $BUILD_PACKAGE: must be Yes or No. Setting to No."
  export BUILD_PACKAGE="No"
fi

# Validate build OS

if [ -z "$BUILDOS" ]; then
  export BUILDOS="Linux"
elif [ "$BUILDOS" != "Linux" -a "$BUILDOS" != "WSL" ]; then
  echo "Unknown BUILDOS value $BUILDOS: must be Linux or WSL"
  CHECKEDC_CONFIG_STATUS="error" 
fi

if [ -z $BUILD_BINARIESDIRECTORY ]; then
  echo "BUILD_BINARIESDIRECTORY not set.  Set it to the directory that will contain the object directory."
  CHECKEDC_CONFIG_STATUS="error" 
fi

if [ -z $BUILD_SOURCESDIRECTORY ]; then
  echo "BUILD_SOURCESDIRECTORY not set.  Set it to the directory that will contain the sources directory."
  CHECKEDC_CONFIG_STATUS="error" 
fi

# Validate that TEST_TARGET_ARCH contains the valid list of targets.
if [ -z "$TEST_TARGET_ARCH" ]; then
  echo "TEST_TARGET_ARCH not set"
  CHECKEDC_CONFIG_STATUS="error"
fi

# First replace ";" with " " to make parsing easier.
TEST_TARGET_ARCH=`echo $TEST_TARGET_ARCH | tr ";" " "`
for TEST_TARGET in $TEST_TARGET_ARCH; do
  if [ "$TEST_TARGET" != "X86_64" -a "$TEST_TARGET" != "ARM" ]; then
    echo "Unknown value $TEST_TARGET in TEST_TARGET_ARCH: must be one of X86_64 or ARM."
    CHECKEDC_CONFIG_STATUS="error"
  fi
done

export LLVM_OBJ_DIR="${BUILD_BINARIESDIRECTORY}/LLVM-${BUILDCONFIGURATION}-${BUILDOS}.obj"

# Validate Test Suite configuration

if [ -z "$TEST_SUITE" ]; then
  echo "TEST_SUITE not set: must be set to one of CheckedC, CheckedC_clang, or CheckedC_LLVM"
  CHECKEDC_CONFIG_STATUS="error" 
elif [ "$TEST_SUITE" != "CheckedC" -a "$TEST_SUITE" != "CheckedC_clang" -a \
       "$TEST_SUITE" != "CheckedC_LLVM" ]; then
  echo "Unknown TEST_SUITE value $TEST_SUITE: must be one of CheckedC, CheckedC_clang, or CheckedC_LLVM"
  CHECKEDC_CONFIG_STATUS="error" 
fi

# SKIP_CHECKEDC_TESTS controls whether to skip the Checked C repo tests
# entirely. This is useful for building/testing a stock (unmodified)
# version of clang/LLVM that does not support Checked C.

if [ -z "$SKIP_CHECKEDC_TESTS" ]; then
  export SKIP_CHECKEDC_TESTS="No"
elif [ "$SKIP_CHECKEDC_TESTS" != "Yes" -a "$SKIP_CHECKEDC_TESTS" != "No" ]; then
  echo Unknown SKIP_CHECKEDC_TESTS value: must be one of Yes or No
  CHECKEDC_CONFIG_STATUS="error"
fi

# set up branch names
if [ -z "$CHECKEDC_BRANCH" ]; then
  export CHECKEDC_BRANCH="master"
fi

if [ -z "$CLANG_BRANCH" ]; then
  export CLANG_BRANCH="master"
fi

if [ -z "$LLVM_TEST_SUITE_BRANCH" ]; then
  export LLVM_TEST_SUITE_BRANCH="master"
fi

# set up source versions (Git commit number)
if [ -z "$CHECKEDC_COMMIT" ]; then
 export CHECKEDC_COMMIT="HEAD"
fi

if [ -z "$CLANG_COMMIT" ]; then
  export CLANG_COMMIT="HEAD"
fi

if [ -z "$LLVM_TEST_SUITE_COMMIT" ]; then
  export LLVM_TEST_SUITE_COMMIT="HEAD"
fi

if [ -z "$BUILD_CPU_COUNT" ]; then
  declare -i NPROC=$(nproc);
  if [ "$BUILDCONFIGURATION" = "Release" ]; then
    export BUILD_CPU_COUNT=$(($NPROC*3/4))
  else
    # Reduce build parallelism for debug builds.
    export BUILD_CPU_COUNT=$(($NPROC*3/8))
  fi
fi

if [ -z "$RUN_LOCAL" ]; then
  export RUN_LOCAL="no"
fi

if [[ -z "$BENCHMARK" ]]; then
  export BMARK=no

else
  LNT_DB_DIR=/usr/local/checkedc/lnt-setup/llvm.lnt.db
  if [ ! -d ${LNT_DB_DIR} ]; then
    echo "No LNT DB found at $LNT_DB_DIR"
    exit 1
  fi
  export BMARK=yes
  export LNT_DB_DIR

  EXTRA_LNT_ARGS=
  for OPT in $METRICS; do
    case "$OPT" in
      PERF)
        EXTRA_LNT_ARGS+="--threads=1 "
        ;;
      COMPILETIME)
        EXTRA_LNT_ARGS+="--build-threads=1 "
        ;;
    esac
  done
  export EXTRA_LNT_ARGS

  if [[ -z "$ONLY_TEST" ]]; then
    ONLY_TEST=""
  fi
  export ONLY_TEST

  if [[ -z "$SAMPLES" ]]; then
    SAMPLES=3
  fi
  export SAMPLES
fi

# LLVM Nightly Tests are enabled when LNT is a non-empty
# string.
if [ -z "$LNT" ]; then
  # Make sure LNT variable is defined so that scripts that require all variables
  # to be defined do not break.
  export LNT=""
  export LNT_RESULTS_DIR=""
  export LNT_SCRIPT=""
else
  export LNT_RESULTS_DIR="${BUILD_BINARIESDIRECTORY}/LNT-Results-${BUILDCONFIGURATION}-${BUILDOS}"
  if [ "$RUN_LOCAL" = "no" ]; then
    # We assume that lnt is installed in /lnt-install on test machines.
    export LNT_SCRIPT=/lnt-install/sandbox/bin/lnt
  fi
fi
 
if [ "$CHECKEDC_CONFIG_STATUS" == "passed" ]; then
  echo "Configured environment variables:"
  echo
  echo " BUILDCONFIGURATION: $BUILDCONFIGURATION"
  echo " BUILD_PACKAGE: $BUILD_PACKAGE"
  echo " BUILDOS: $BUILDOS"
  echo " TEST_TARGET_ARCH: $TEST_TARGET_ARCH"
  echo " TEST_SUITE: $TEST_SUITE"
  echo " SKIP_CHECKEDC_TESTS: $SKIP_CHECKEDC_TESTS"
  echo " LNT: $LNT"
  echo " LNT_SCRIPT: $LNT_SCRIPT"
  echo " RUN_LOCAL: $RUN_LOCAL"
  echo " BENCHMARK: $BMARK"
  echo " LNT_DB_DIR: $LNT_DB_DIR"
  echo
  echo " Directories:"
  echo "  BUILD_SOURCESDIRECTORY: $BUILD_SOURCESDIRECTORY"
  echo "  BUILD_BINARIESDIRECTORY: $BUILD_BINARIESDIRECTORY"
  echo "  LLVM_OBJ_DIR: $LLVM_OBJ_DIR"
  echo "  LNT_RESULTS_DIR: $LNT_RESULTS_DIR"
  echo 
  echo " Branch and commit information:"
  echo "  CLANG_BRANCH: $CLANG_BRANCH"
  echo "  CLANG_COMMIT: $CLANG_COMMIT"
  echo "  CHECKEDC BRANCH: $CHECKEDC_BRANCH"
  echo "  CHECKEDC_COMMIT: $CHECKEDC_COMMIT"
  echo "  LLVM_TEST_SUITE_BRANCH: $LLVM_TEST_SUITE_BRANCH"
  echo "  LLVM_TEST_SUITE_COMMIT: $LLVM_TEST_SUITE_COMMIT"
  echo
  echo " BUILD_CPU_COUNT: $BUILD_CPU_COUNT"
else
  echo "Configuration of environment variables failed"
fi
