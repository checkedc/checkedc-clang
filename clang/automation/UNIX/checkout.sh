#!/usr/bin/env bash

# For a description of the config variables set/used here,
# see automation/Windows/build-and-test.bat.

source ./config-vars.sh

echo "======================================================================"
echo "Checking out checkedc-clang sources"
echo "======================================================================"

set -ue
set -o pipefail

function clone_or_update {
  local dir="$PWD/$1"
  local url="$2"
  local branch="$3"

  if [ ! -d $dir/.git ]; then
    echo "======================================================================"
    echo "Cloning $url to $dir"
    echo "======================================================================"
    git clone $url $dir
  else
    echo "======================================================================"
    echo "Updating $dir"
    echo "======================================================================"
    cd $dir &&
    git fetch origin
  fi

  echo "======================================================================"
  echo "Checking out branch $branch for $url"
  echo "======================================================================"
  cd $dir &&
  git checkout -f $branch &&
  git pull -f origin $branch
}

# Make build dir.
mkdir -p "$LLVM_OBJ_DIR"

if [ -n "$LNT" ]; then
  rm -fr "$LNT_RESULTS_DIR"
  mkdir -p "$LNT_RESULTS_DIR"
  if [ ! -e "$LNT_SCRIPT" ]; then
    echo "LNT script is missing from $LNT_SCRIPT"
    exit 1
  fi
fi
cd "$BUILD_SOURCESDIRECTORY"

# Check out Clang
clone_or_update checkedc-clang https://github.com/Microsoft/checkedc-clang "$CLANG_BRANCH"

# Check out Checked C Tests
clone_or_update checkedc-clang/llvm/projects/checkedc-wrapper/checkedc https://github.com/Microsoft/checkedc "$CHECKEDC_BRANCH"

# Check out LLVM test suite
if [ -n "$LNT" ]; then
  clone_or_update llvm-test-suite https://github.com/Microsoft/checkedc-llvm-test-suite "$LLVM_TEST_SUITE_BRANCH"
fi

set +ue
set +o pipefail
