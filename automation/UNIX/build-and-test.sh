#!/usr/bin/env bash

set -x

source ./config-vars.sh
if [ "${CHECKEDC_CONFIG_STATUS}" == "error" ]; then
  exit 1;
fi

set -ue
set -o pipefail

./setup-files.sh
./run-cmake.sh
./test-clang.sh
./test-lnt.sh
./build-package.sh
