# `3c`: Command-line tool for conversion of C code to Checked C

The `3c` tool is a command-line interface to the 3C software for conversion of C code to Checked C.  See the [3C readme](../../docs/checkedc/3C/README.md) (for general information about 3C) and the [build instructions](../../docs/checkedc/3C/INSTALL.md).  This document describes how to use `3c`.  It assumes that you have added the `build/bin` directory containing the `3c` executable to your `$PATH`.

## Workflow

`3c` supports an iterative workflow for converting a C program to Checked C as follows:

1. Run `3c`.  It reads a set of `.c` files and the header files they transitively include (these files may be written in Checked C or, as a special case, plain C), performs a whole-program static analysis to infer as many additions or corrections to Checked C annotations as possible, and writes out the updated files.  `3c` accepts various options to control the assumptions it makes and the types of changes it makes on a given pass.

2. Review the changes made by `3c` as well as what it didn't change and any diagnostics it produced.  Manually add annotations to help Checked C verify the safety of your existing code, edit your code to make its safety easier to verify, and/or mark parts of the code that you don't want to try to verify with Checked C (because you know they are beyond what Checked C can handle or verifying them just isn't worthwhile to you at the moment).

3. Repeat until you are satisfied.

For a complete, worked example, see [our tutorial](https://github.com/correctcomputation/checkedc-tiny-bignum-c).

The Microsoft `checkedc-clang` wiki has lots of helpful [advice on porting C programs to Checked C](https://github.com/Microsoft/checkedc/wiki/Legacy-Conversion-Tips), not specific to 3C.

## Basic usage

The task of `3c` is complicated by the fact that the build system for a typical C codebase will call the C compiler once per `.c` file, possibly with different options for each file (include directories, preprocessor definitions, etc.), and then link all the object files at the end.  A Clang-based whole-program analysis like 3C needs to process all `.c` files at once _with the correct options for each_.  To achieve this, you get your build system to produce a Clang "compilation database" (a file named `compile_commands.json`) containing the list of `.c` files and the options for each ([how to do this depends on the build system](../../docs/JSONCompilationDatabase.rst)), and then 3C reads this database.

However, in a simpler setting, you can manually run `3c` on one or more source files, for example:

```
3c -alltypes -output-postfix=checked foo.c bar.c
```

This will write the new version of each file `f.c` to `f.checked.c` in the same directory (or `f.h` to `f.checked.h`).  If `f.checked.c` would be identical to `f.c`, it is not created.

The `-alltypes` option causes `3c` to try to infer array types.  We want to make this the default but haven't done so yet because it breaks some things.

## More about file handling and compiler options

As an additional safeguard, `f.checked.c` or `f.checked.h` is not written if it is outside the _base directory_, which defaults to the working directory but can be overridden with the `-base-dir` option.  This can help ensure that you don't unintentionally modify external libraries (though if their header files are not [annotated for Checked C](#annotated-headers), your ability to convert your own program to Checked C may be limited).  However, there is a bug in the way the base directory check handles `..` path components in `-I` directories and `#include` paths (terrible, we know!), so until we can fix the bug, please avoid using `..` path components (e.g., use `-I` with an absolute path instead).

You can ignore the errors about a compilation database not being found.  You can specify a single set of compiler options to use for all files by prefixing them with `-extra-arg-before=`, for example:

```
3c -alltypes -output-postfix=checked -extra-arg-before=-Isome/include/path foo.c bar.c
```

(If you were using a compilation database, such "extra" options would be added to any compiler options in the database.)

## Annotated headers

If a Checked C program uses an element (function, variable, type, etc.) from an external library and the only available declaration of the element uses unsafe C pointers, the Checked C code will have to use unsafe pointers to interact with that element.  Having a declaration that uses Checked C annotations (even if the safety of the implementation with respect to those annotations has not been verified) will enable both you and 3C to write better Checked C code.

In the future, we may have some scheme analogous to [DefinitelyTyped](https://definitelytyped.org/) to distribute Checked C headers for existing C libraries.  In the meantime, the Checked C project maintains annotated versions of the most common "system" header files (`stdio.h`, etc.).  The annotated header files do not include all elements, but they `#include` your system's original header files, so your program can still use unannotated elements (though it may have to use unsafe pointers to do so).  If you followed the [build instructions](../../docs/checkedc/3C/INSTALL.md), the annotated header files should be present in `llvm/projects/checkedc-wrapper/checkedc/include`.  The Microsoft `checkedc-clang` wiki has [a bit more information about these header files](https://github.com/Microsoft/checkedc-clang/wiki/Checked-C-clang-user-manual#header-files).

Currently, the annotated header files are named with a `_checked.h` suffix, e.g., `stdio_checked.h`, so that `stdio_checked.h` can `#include <stdio.h>` without causing infinite recursion.  We hope to switch to `#include_next` and remove the suffix soon.  In the meantime, you have to modify your code to `#include <stdio_checked.h>` instead of `stdio.h` and so forth.  The `clang/tools/3c/utils/update-includes.py` tool will do this for you.  It takes one argument: the name of a file containing a list of paths of `.c` and `.h` files to be updated.  In the example of the previous section, you would run:

```
cat >list <<EOL
foo.c
bar.c
EOL
PATH/TO/clang/tools/3c/utils/update-includes.py list
```

## Useful 3C-specific options

An incomplete list of useful options specific to `3c` (ordinary compiler options may be passed via `-extra-arg-before` as stated [above](#more-about-file-handling-and-compiler-options)):

- `-warn-root-cause`: Show information about the _root causes_ that prevent `3c` from converting unsafe pointers (`T *`) to safe ones (`_Ptr<T>`, etc.).
