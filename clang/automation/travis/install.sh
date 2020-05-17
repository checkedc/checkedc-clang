#!/usr/bin/env bash

set -ue
set -o pipefail

CMAKE_REQ_VERS=3.7
if cmake --version | grep $CMAKE_REQ_VERS > /dev/null; then
  export CMAKE_OUR_BIN=`which cmake`
  echo "Using System CMake: ${CMAKE_OUR_BIN}"
else
  if [ ! -x cmake/${CMAKE_DIR}/${CMAKE_BIN_DIR}/cmake ]; then
    mkdir -p cmake
    curl -o cmake.tar.gz  $CMAKE_URL
    tar -xzf cmake.tar.gz
    mv $CMAKE_DIR cmake
  fi

  export CMAKE_OUR_BIN="$(pwd)/cmake/${CMAKE_DIR}/${CMAKE_BIN_DIR}/cmake"
  export PATH="$(pwd)/cmake/${CMAKE_DIR}/${CMAKE_BIN_DIR}:$PATH"
  echo "Using Own CMake: ${CMAKE_OUR_BIN}"
  $CMAKE_OUR_BIN --version | grep -q $CMAKE_REQ_VERS
fi

# Virtualenv for LNT (cached)
if [ ! -x llvm.lnt.ve/bin/python ]; then
  virtualenv -q ./llvm.lnt.ve
fi
export LNT_VE_DIR="$(pwd)/llvm.lnt.ve"

set +ue
set +o pipefail
