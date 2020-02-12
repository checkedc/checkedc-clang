#!/usr/bin/env bash

# For a description of the config variables set/used here,
# see automation/Windows/build-and-test.bat.

source ./config-vars.sh

echo "======================================================================"
echo "Running unit tests using lit for the Checked C compiler"
echo "======================================================================"

set -ue
set -o pipefail

cd ${LLVM_OBJ_DIR}

if [ "${SKIP_CHECKEDC_TESTS}" != "Yes" ]; then
  echo "======================================================================"
  echo "Running ninja check-checkedc"
  echo "======================================================================"
  ninja check-checkedc
fi

if [ "${TEST_SUITE}" == "CheckedC_clang" ]; then
  echo "======================================================================"
  echo "Running ninja check-clang"
  echo "======================================================================"
  ninja check-clang
elif [ "${TEST_SUITE}" == "CheckedC_LLVM" ]; then
  echo "======================================================================"
  echo "Running ninja check-all"
  echo "======================================================================"
  ninja check-all
fi

set +ue
set +o pipefail
