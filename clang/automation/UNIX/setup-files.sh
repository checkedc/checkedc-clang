#!/usr/bin/env bash

set -ue
set -o pipefail
set -x

function clone_or_update {
  local dir="${PWD}/${1}"
  local url="${2}"
  local branch="${3}"
  local commit="${4}"

  if [ ! -d ${dir}/.git ]; then
    echo "Cloning ${url} to ${dir}"
    git clone ${url} ${dir}
  else
    echo "Updating ${dir}"
    (cd ${dir}; git fetch origin)
  fi

  echo "Switching ${dir} to ${branch}"
  (cd ${dir}; git checkout -f $branch; git pull -f origin $branch; git checkout "$commit")
}

# Make Build Dir
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
clone_or_update checkedc-clang https://github.com/Microsoft/checkedc-clang "$CLANG_BRANCH" "$CLANG_COMMIT"

# Check out Checked C Tests
clone_or_update checkedc-clang/llvm/projects/checkedc-wrapper/checkedc https://github.com/Microsoft/checkedc
"$CHECKEDC_BRANCH" "$CHECKEDC_COMMIT"

# Check out LLVM test suite
if [ -n "$LNT" ]; then
  clone_or_update llvm-test-suite https://github.com/Microsoft/checkedc-llvm-test-suite "$LLVM_TEST_SUITE_BRANCH" "$LLVM_TEST_SUITE_COMMIT"
fi

# Make package directory if building a package
cd "$LLVM_OBJ_DIR"
if [ "${BUILD_PACKAGE}" == "Yes" ]; then
    mkdir -p "package"
    rm -rf "package/*"
fi

set +ue
set +o pipefail
