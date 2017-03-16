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

cd "$BUILD_SOURCESDIRECTORY"

# Check out LLVM
clone_or_update llvm https://github.com/Microsoft/checkedc-llvm "$LLVM_BRANCH" "$LLVM_COMMIT"

# Check out Clang
clone_or_update llvm/tools/clang https://github.com/Microsoft/checkedc-clang "$CLANG_BRANCH" "$CLANG_COMMIT"

# Check out Checked C Tests
clone_or_update llvm/projects/checkedc-wrapper/checkedc https://github.com/Microsoft/checkedc "$CHECKEDC_BRANCH" "$CHECKEDC_COMMIT"

## Comment out LNT-related stuff for now.  We are not ready to automate that yet.
# Check out LNT
# clone_or_update lnt https://github.com/llvm-mirror/lnt master

# Check out Test Suite
# clone_or_update llvm-test-suite https://github.com/Microsoft/checkedc-llvm-test-suite master


# Install lnt into the virtualenv we set up in install.sh
#(cd lnt;
# $LNT_VE_DIR/bin/python setup.py -q install)

set +ue
set +o pipefail
