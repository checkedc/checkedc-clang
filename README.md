# The Checked C clang repo

<<<<<<< HEAD
This directory and its sub-directories contain the source code for LLVM,
a toolkit for the construction of highly optimized compilers,
optimizers, and run-time environments.
=======
This repo contains a version of the LLVM/Clang toolchain that has been modified to support Checked C. 
Checked C extends C with bounds checking and improved type-safety.
The Checked C specification is available at the
[Checked C repo release page](https://github.com/checkedc/checkedc/releases).
>>>>>>> main

<!---
## Announcements

### Source code update

<<<<<<< HEAD
Taken from [here](https://llvm.org/docs/GettingStarted.html).
=======
On Feb 19, 2021 we updated the checkedc-clang sources to upstream release_110,
specifically [this](https://github.com/llvm/llvm-project/commit/2e10b7a39b930ef8d9c4362509d8835b221fbc0a) commit.
>>>>>>> main

On Feb 18, 2020 we updated the checkedc-clang sources to upstream release_90,
specifically [this](https://github.com/llvm/llvm-project/commit/c89a3d78f43d81b9cff7b9248772ddf14d21b749) commit.
--->

## Trying out Checked C

<<<<<<< HEAD
The LLVM project has multiple components. The core of the project is
itself called "LLVM". This contains all of the tools, libraries, and header
files needed to process intermediate representations and convert them into
object files. Tools include an assembler, disassembler, bitcode analyzer, and
bitcode optimizer. It also contains basic regression tests.

C-like languages use the [Clang](http://clang.llvm.org/) frontend. This
component compiles C, C++, Objective-C, and Objective-C++ code into LLVM bitcode
-- and from there into object files, using LLVM.
=======
You can install the Checked C compiler and the 3C tool
from the [Checked C LLVM Project releases page](https://github.com/checkedc/checkedc-llvm-project/releases).
There are versions available for Ubuntu 22.04, Windows 10/11, and MacOS.
The compiler user
manual is [here](https://github.com/checkedc/checkedc-llvm-project/wiki/Checked-C-clang-user-manual).
For more information on Checked C and pointers to example code, see the
[Checked C wiki](https://github.com/checkedc/checkedc/wiki).
If you want to build your own copy of the compiler, see the directions on the
[Checked C LLVM Project wiki](https://github.com/checkedc/checkedc-llvm-project/wiki).

You can use `clangd` built from this repository to get similar IDE support for
editing Checked C code as upstream `clangd` provides for C code. For example,
you can jump to definition/references and get a real-time display of errors and
warnings, etc. Here is [more information about Checked C's
`clangd`](clang/docs/checkedc/clangd.md).
>>>>>>> main

## 3C: Semi-automated conversion of C code to Checked C

This repository includes a tool called 3C that partially automates the
conversion of C code to Checked C. Quick documentation links:

<<<<<<< HEAD
The LLVM Getting Started documentation may be out of date. The [Clang
Getting Started](http://clang.llvm.org/get_started.html) page might have more
accurate information.
=======
* [General information](clang/docs/checkedc/3C/README.md), including development
  status and how to contribute
>>>>>>> main

* [Build instructions](clang/docs/checkedc/3C/INSTALL.md)

* [Usage instructions for the `3c` command-line tool](clang/tools/3c/README.md)

## More information

For more information on the Checked C clang compiler, see the [Checked C LLVM Project
wiki](https://github.com/checkedc/checkedc-llvm-project/wiki).

## Automated testing status

[![Checked C Clang CI [Linux]](https://github.com/checkedc/workflows/actions/workflows/check-clang-linux.yml/badge.svg)](https://github.com/checkedc/workflows/actions/workflows/check-clang-linux.yml)

[![Checked C Clang CI [MacOS]](https://github.com/checkedc/workflows/actions/workflows/checkedc-clang-macos.yml/badge.svg)](https://github.com/checkedc/workflows/actions/workflows/checkedc-clang-macos.yml)

<<<<<<< HEAD
     * ``cmake -S llvm -B build -G <generator> [options]``
=======
[![Checked C Clang CI [WINDOWS]](https://github.com/checkedc/workflows/actions/workflows/check-clang-windows.yml/badge.svg)](https://github.com/checkedc/workflows/actions/workflows/check-clang-windows.yml)

## Contributing

We welcome contributions to the Checked C project. To get involved in the
project, see [Contributing to Checked
C](https://github.com/checkedc/checkedc/blob/main/CONTRIBUTING.md).
>>>>>>> main

For code contributions, we follow the standard [Github
workflow](https://guides.github.com/introduction/flow/). See [Contributing to
Checked C](https://github.com/checkedc/checkedc/blob/main/CONTRIBUTING.md)
for more detail.

## Code of conduct

<<<<<<< HEAD
        Some common options:

        * ``-DLLVM_ENABLE_PROJECTS='...'`` and ``-DLLVM_ENABLE_RUNTIMES='...'`` ---
          semicolon-separated list of the LLVM sub-projects and runtimes you'd like to
          additionally build. ``LLVM_ENABLE_PROJECTS`` can include any of: clang,
          clang-tools-extra, cross-project-tests, flang, libc, libclc, lld, lldb,
          mlir, openmp, polly, or pstl. ``LLVM_ENABLE_RUNTIMES`` can include any of
          libcxx, libcxxabi, libunwind, compiler-rt, libc or openmp. Some runtime
          projects can be specified either in ``LLVM_ENABLE_PROJECTS`` or in
          ``LLVM_ENABLE_RUNTIMES``.

          For example, to build LLVM, Clang, libcxx, and libcxxabi, use
          ``-DLLVM_ENABLE_PROJECTS="clang" -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi"``.

        * ``-DCMAKE_INSTALL_PREFIX=directory`` --- Specify for *directory* the full
          path name of where you want the LLVM tools and libraries to be installed
          (default ``/usr/local``). Be careful if you install runtime libraries: if
          your system uses those provided by LLVM (like libc++ or libc++abi), you
          must not overwrite your system's copy of those libraries, since that
          could render your system unusable. In general, using something like
          ``/usr`` is not advised, but ``/usr/local`` is fine.

        * ``-DCMAKE_BUILD_TYPE=type`` --- Valid options for *type* are Debug,
          Release, RelWithDebInfo, and MinSizeRel. Default is Debug.

        * ``-DLLVM_ENABLE_ASSERTIONS=On`` --- Compile with assertion checks enabled
          (default is Yes for Debug builds, No for all other build types).

      * ``cmake --build build [-- [options] <target>]`` or your build system specified above
        directly.

        * The default target (i.e. ``ninja`` or ``make``) will build all of LLVM.

        * The ``check-all`` target (i.e. ``ninja check-all``) will run the
          regression tests to ensure everything is in working order.

        * CMake will generate targets for each tool and library, and most
          LLVM sub-projects generate their own ``check-<project>`` target.

        * Running a serial build will be **slow**. To improve speed, try running a
          parallel build. That's done by default in Ninja; for ``make``, use the option
          ``-j NNN``, where ``NNN`` is the number of parallel jobs to run.
          In most cases, you get the best performance if you specify the number of CPU threads you have.
          On some Unix systems, you can specify this with ``-j$(nproc)``.

      * For more information see [CMake](https://llvm.org/docs/CMake.html).

Consult the
[Getting Started with LLVM](https://llvm.org/docs/GettingStarted.html#getting-started-with-llvm)
page for detailed information on configuring and compiling LLVM. You can visit
[Directory Layout](https://llvm.org/docs/GettingStarted.html#directory-layout)
to learn about the layout of the source code tree.

## Getting in touch

Join [LLVM Discourse forums](https://discourse.llvm.org/), [discord chat](https://discord.gg/xS7Z362) or #llvm IRC channel on [OFTC](https://oftc.net/).

The LLVM project has adopted a [code of conduct](https://llvm.org/docs/CodeOfConduct.html) for
participants to all modes of communication within the project.
=======
This project has adopted a
[code of conduct](https://github.com/checkedc/checkedc/blob/main/CODE_OF_CONDUCT.md).
>>>>>>> main
