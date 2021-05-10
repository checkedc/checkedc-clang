# `3c`: Command-line tool for conversion of C code to Checked C

The `3c` tool is a command-line interface to the 3C software for
conversion of C code to Checked C. See the [3C
readme](../../docs/checkedc/3C/README.md) (for general information
about 3C) and the [build
instructions](../../docs/checkedc/3C/INSTALL.md). This document
describes how to use `3c`.

As mentioned in the build instructions, the `3c` executable is in the
`build/bin` directory of your working tree. To use the commands below,
either add that directory to your `$PATH` or replace `3c` with
whatever other means you wish to use to run the executable (e.g., an
absolute path or a wrapper script).

## Workflow

`3c` supports an iterative workflow for converting a C program to
Checked C as follows:

1. Run `3c`. It reads a set of `.c` files and the header files they
transitively include (these files may be written in Checked C or, as a
special case, plain C), performs a whole-program static analysis to
infer as many additions or corrections to Checked C annotations as
possible, and writes out the updated files. `3c` accepts various
options to control the assumptions it makes and the types of changes
it makes on a given pass.

2. Review the changes made by `3c` as well as what it didn't change
and any diagnostics it produced. Manually add annotations to help
Checked C verify the safety of your existing code, edit your code to
make its safety easier to verify, and/or mark parts of the code that
you don't want to try to verify with Checked C (because you know they
are beyond what Checked C can handle or verifying them just isn't
worthwhile to you at the moment). Compile your program with the
Checked C compiler (`clang`) to check your work.

3. Repeat until you are satisfied.

For a complete, worked example, see [our
tutorial](https://github.com/correctcomputation/checkedc-tiny-bignum-c).

The Microsoft `checkedc-clang` wiki has lots of helpful [advice on
porting C programs to Checked
C](https://github.com/Microsoft/checkedc/wiki/Legacy-Conversion-Tips),
not specific to 3C.

## Basic usage

The task of `3c` is complicated by the fact that the build system for
a typical C codebase will call the C compiler once per `.c` file,
possibly with different options for each file (include directories,
preprocessor definitions, etc.), and then link all the object files at
the end. A Clang-based whole-program analysis like 3C needs to process
all `.c` files at once _with the correct options for each_. To achieve
this, you get your build system to produce a Clang "compilation
database" (a file named `compile_commands.json`) containing the list
of `.c` files and the options for each ([how to do this depends on the
build system](../../docs/JSONCompilationDatabase.rst)), and then 3C
reads this database.

For simple tests, you can run `3c` on one or more source files without
a compilation database. For example, the following command will
convert `foo.c` and print the new version to standard output:

```
3c -addcr -alltypes foo.c --
```

The `--` at the end of the command line indicates that you _do not_
want to use a compilation database. This is important to ensure `3c`
doesn't automatically detect a compilation database that you don't
want to use. You can add compiler options that you want to use for all
files after the `--`, such as `-I` for include directories, etc.

This "stdout mode" only supports a single source file. If you have
multiple files, you must use one of the modes that writes the output
to files. You can specify either a string to be inserted into the
names of the new files or a directory under which they should be
written. For example, this command:

```
3c -addcr -alltypes -output-postfix=checked foo.c bar.c --
```

will write the new versions to `foo.checked.c` and `bar.checked.c`. If
one of these files is not created, it means the original file needs no
changes. If `foo.c` includes a header file `foo.h`, then `3c` will
automatically include it in the conversion and write the new version
of it to `foo.checked.h` if there are changes; you don't need to
specify header files separately on the command line, just as they
wouldn't have their own entries in a compilation database.

The `-output-postfix` mode may be convenient for running tools such as
`diff` on individual files by hand. Alternatively, this command:

```
3c -addcr -alltypes -output-dir=/path/to/new foo.c bar.c --
```

will write the new versions to `/path/to/new/foo.c` and
`/path/to/new/bar.c`. You can then run something like `diff -ru .
/path/to/new` to diff all the files at once (_without_ the `-N` option
because many files in your starting directory may not have new
versions written out).

We typically recommend using the `-addcr` and `-alltypes` options, as
shown above. Here's what they mean:

- `-addcr` makes 3C add _checked region_ annotations to your code,
  which make it easier to visualize the parts where "unsafe"
  operations are being performed; we plan to make it the default as
  soon as it is stable enough.

- By default, 3C tries to ensure that its output always passes the
  Checked C compiler's type checker. The `-alltypes` option enables
  some 3C features that try to generate annotations that are closer to
  what you ultimately want but may not pass the type checker right
  away without manual corrections. Currently, the only such feature is
  inference of array types. 3C's determination of whether a pointer
  points to an array is generally reliable, but it isn't always able
  to infer correct bounds that will pass the type checker, so when
  `-alltypes` is off, 3C will infer `_Ptr` types as normal but leave
  array pointers unchecked. With `-alltypes`, 3C infers `_Array_ptr`
  and `_Nt_array_ptr` types with its best guess of the bounds.

## The base directory

The source files you specify on the command line may transitively
include many header files. You probably want `3c` to convert header
files that are part of your project (indeed, you generally won't be
able to convert program elements in your source files without also
converting their declarations in your header files), but you probably
_don't_ want `3c` to modify header files for libraries that are
outside your control.

To achieve this, `3c` uses the concept of a _base directory_: an
ancestor directory that defines the set of files it is allowed to
modify. The base directory defaults to the working directory and can
be changed with the `-base-dir` option. All files specified on the
command line must be under the base directory. Transitively included
files that are under the base directory will be converted, whereas 3C
tries to constrain its analysis to avoid needing to modify
transitively included files that are outside the base directory.
([Some unusual cases are not yet handled
correctly](https://github.com/correctcomputation/checkedc-clang/issues/387),
and if 3C tries to modify a file outside the base directory as a
result, it reports an error.)

## Annotated headers

If a Checked C program uses an element (function, variable, type,
etc.) from an external library and the only available declaration of
the element uses unsafe C pointers, the Checked C code will have to
use unsafe pointers to interact with that element. Having a
declaration that uses Checked C annotations (even if the safety of the
implementation with respect to those annotations has not been
verified) will enable both you and 3C to write better Checked C code.

In the future, we may have some scheme analogous to
[DefinitelyTyped](https://definitelytyped.org/) to distribute Checked
C headers for existing C libraries. In the meantime, the Checked C
project maintains annotated versions of the most common "system"
header files (`stdio.h`, etc.). The annotated header files do not have
declarations for all elements, but they `#include` your system's
original header files, so your program can still use unannotated
elements (though it may have to use unsafe pointers to do so). If you
followed the [build instructions](../../docs/checkedc/3C/INSTALL.md),
the annotated header files should be present in
`llvm/projects/checkedc-wrapper/checkedc/include`. The Microsoft
`checkedc-clang` wiki has [a bit more information about these header
files](https://github.com/Microsoft/checkedc-clang/wiki/Checked-C-clang-user-manual#header-files).

Since April 2021, a directive such as `#include <stdio.h>` will
automatically include the Checked C header along with the system
header, so you no longer need to modify your code to `#include
<stdio_checked.h>`. Here is [more information about this
feature](../../docs/checkedc/rfc/Include-Checked-Hdrs.md).

## Useful 3C-specific options

An incomplete list of useful options specific to `3c` (ordinary
compiler options may be passed after the `--` as stated above):

- `-warn-root-cause`: Show information about the _root causes_ that
  prevent `3c` from converting unsafe pointers (`T *`) to safe ones
  (`_Ptr<T>`, etc.).

See `3c -help` for more.
