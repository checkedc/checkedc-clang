#!/usr/bin/env bash

set -x

# For a description of the config variables set/used here,
# see automation/Windows/build-and-test.bat.

source ./config-vars.sh
if [ "${CHECKEDC_CONFIG_STATUS}" == "error" ]; then
  exit 1;
fi

set -ue
set -o pipefail

#./setup-files.sh
#./run-cmake.sh
#./test-clang.sh
./test-lnt.sh
#./build-package.sh
