# Conversion Utilities

This directory contains a set of utilities to help with converting a codebase. Python 2.7 required.

## convert-commands.py
This script takes two arguments `compileCommands` (the path to the `compile_commands.json` for the configuration you plan to convert) and `progName` (which is checked-c-convert). It reads the `compile_commands.json` (which must match the fields produced by CMake's versions of such files) and produces an output file which contains a command-line invocation of `progName` with some flags and all `.c` files which are compiled by this configuration (and thus should be converted by `checked-c-convert`). This file is currently saved as `convert.sh` and can be run directly as a shell script.

## update-includes.py

Given a file that contains a list of `.c` or `.h` filenames (such as the one produced by `convert-commands.py` or generated with the POSIX `find > outfile` command), this script will look for `#include` statements in each file in the list that `#include` headers for which there are CheckedC replacements (e.g. stdlib, math) and substitutes in those replacements. This rewrites the files in place!
Details: 
- The default directory to look for the CheckedC replacement headers is documented at the top of the file, but the optional argument `--includeDir` allows specification of a different directory.
- This script will strip `\` characters and whitespace off the end of each line but expects the filename to be at the beginning of the line.
- Since there are likely to be many source files for the program to iterate though, and the source files may be long, there is a preprocessing step to build the FindReplace function for the checkedC headers. This allows doing all replacements in a single pass through the source file.
