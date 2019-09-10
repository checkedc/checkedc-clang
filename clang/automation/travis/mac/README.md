# Build System for Checked C

This directory provides mac-specific overrides for the linux-based
build scripts.

## Requirements

The scripts in this directory assume Mac OS X.

Currently we assume that you have the following installed already:
- Homebrew package manager
- a C/C++ compiler (clang/gcc)
- Make
- python 2.7 and virtualenv
- awk. Because bash doesn't have a `minimum` function
- Anything else you need installed to build Clang.
