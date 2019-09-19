#!/usr/bin/env bash

# Run with sudo

set -ue
set -o pipefail

apt-get install tcl bison zlib1g-dev python2.7 python-virtualenv

set +ue
set +o pipefail
