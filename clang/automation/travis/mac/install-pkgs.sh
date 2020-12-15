#!/usr/bin/env bash

set -ue
set -o pipefail

# Installs system packages.
# python 2.7, tcl and zlib1g-dev should be installed by default

brew install bison

# TODO: python-virtualenv?

set +ue
set +o pipefail
