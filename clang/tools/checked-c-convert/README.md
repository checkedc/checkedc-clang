# checked-c-convert

This document describes the high level design and usage of the 
`checked-c-convert` tool, which automatically converts C code into Checked C
code. 

`checked-c-convert` is based on the LLVM libtooling infrastructure, so the
command line behaves very similarly to those tools. It is also a source to 
source re-writer, and there are a few different options for how it outputs
changed programs. For small, single-file programs, `checked-c-convert` can
output to `stdout`. For larger, multi-file programs, `checked-c-convert` 
can output new files with a specified file name postfix.

## Usage
There are two different ways to use `checked-c-convert`, either on a set of 
manually specified files with a manually specified command line, or, using
a `compile_commands.json` compilation database. 

### Command Line
Invoke `checked-c-convert` on the command line as:

`$ checked-c-convert -p path/to/directory/containing/compile_commands.json path/to/source/file/to/convert.c`

### `compile_commands.json` database
CMake's configuration tool can generate, for certain targets, a `compile_commands.json` file listing the instructions for the compiler during build. Produce this with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` added to the configuration call to cmake. Note: as of CMake 3.12, only the Makefile and Ninja generators support this command.

### Utility to generate source list from compile_commands
There's a small Python2 script in tools/checked-c-convert/utils that will take the `compile_commands` and generate a `checked-c-convert` command line invocation that contains all the source files needed as arguments. Run it as:
`python2.7 run.py path/to/compile_commands.json checked-c-convert`

## Design Notes
The tool performs a global best-effort-whole-program flow-insensitive 
context-insensitive unification-based constraint analysis to identify
variables that maybe converted into Checked C `ptr<T>` types. In general,
the tool aims to be conservative and to not make radical changes to the 
structure of the program. It is hoped that the programs output from 
`checked-c-convert` are recognizable by the authors of the input 
programs.

## Example usage
`checked-c-convert -verbose -dump-stats -output-postfix=checked ~/src/hello.c ~/src/utils/myUtils.c`

## Tests
There are unit tests for the constraint solver and functional tests for
the rewriter. The functional tests use the `llvm-lit` testing infrastructure
and the unit tests use the `google-test` infrastructure as a stand-alone 
executable. 

To invoke the functional tests, run the following command from the base of the
`checkedc-llvm` repo:

	llvm-lit tools/clang/test/CheckedCRewriter/simple_locals.c

To invoke the unit tests, find the executable `RewriterTest` and run it with
no arguments.
