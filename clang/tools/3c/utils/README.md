# Conversion Utilities

This directory contains a set of utilities to help with converting a codebase. Python 2.7 required.

## convert-commands.py
This script takes two named arguments `compileCommands` (`-cc`)
(the path to the `compile_commands.json` for the configuration you plan to convert) 
and `progName` (`-p`), which is the path to the 3c binary. 
It reads the `compile_commands.json` 
(which must match the fields produced by CMake's versions of such files) and 
produces an output file which contains a command-line invocation of `progName` with 
some flags and all `.c` files which are compiled by this configuration 
(and thus should be converted by `3c`). 
This file is currently saved as `convert_all.sh` and can be run directly as a shell script.
The `convert-commands.py` also creates `convert_individual.sh` file that 
contains the commands to run the `3c` tool on individual source files.

### Example:
```
python convert-commands.py --cc <path_to_compile_commands.json> -p <path_to_the_3c_binary>
```

### Generating `compile_commands.json`
#### Using `cmake`
Use the CMAKE_EXPORT_COMPILE_COMMANDS flag. You can run
```
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ...
```
or add the following line to your CMakeLists.txt script:
```
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```
The `compile_commands.json` file will be put into the build directory.
#### Using `Bear` (Recommended)
For `make` and `cmake` based build systems, you can use `Bear`.

Install Bear from: https://github.com/rizsotto/Bear

Prepend `bear` to your make command i.e., if you were running `make -j4` 
then run  `bear make -j4`. 
The `compile_commands.json` file will be put into the current directory.


## update-includes.py

Given a file that contains a list of `.c` or `.h` filenames 
(such as the one produced by `convert-commands.py` or generated with the POSIX `find > outfile` command), 
this script will look for `#include` statements in each file in the list 
that `#include` headers for which there are CheckedC replacements (e.g. stdlib, math) 
and substitutes in those replacements. **This rewrites the files in place!**

**Hint: To get all the `.c` and `.h` files in a directory recursively use the following command: `find <path_to_the_directory> -regex '.*/.*\.\(c\|h\)$'`**


Details: 
- The default directory to look for the CheckedC replacement headers is documented at the top of the file, but the optional argument `--includeDir` allows specification of a different directory.
- This script will strip `\` characters and whitespace off the end of each line but expects the filename to be at the beginning of the line.
- Since there are likely to be many source files for the program to iterate though, and the source files may be long, there is a preprocessing step to build the FindReplace function for the checkedC headers. This allows doing all replacements in a single pass through the source file.
