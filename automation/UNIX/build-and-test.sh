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

echo RUN_LOCAL:$RUN_LOCAL

if [ "$RUN_LOCAL" = "yes" ]; then
  ./setup-files.sh
  ./run-cmake.sh
  ./test-clang.sh
fi

./test-lnt.sh

if [ "$RUN_LOCAL" = "yes" ]; then
  ./build-package.sh
fi
