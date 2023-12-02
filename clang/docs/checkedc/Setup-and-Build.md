# Setting up your machine and building clang

Note: The automation scripts used to build and test the Checked C compiler have
now been moved to their own
[repo](https://github.com/checkedc/checkedc-automation). We use CMake with
Ninja for both Linux and Windows builds.

## Setting up your machine

See the clang [Getting started guide](http://clang.llvm.org/get_started.html)
for information on how to set up your machine.  For Windows machines,
follow the steps below instead.

For Linux, install CMake 3.8 or later. For Windows, CMake is bundled as part of
your Visual Studio install.

If you want to run tests, you will need a Python version between 3.7 and 3.11
installed.  Python 3.12 contains changes that break our old version of the `lit`
testing tool.


### Setting up a Windows machine

We recommend that you use a 64-bit version of Windows. We have found that the
32-bit hosted Visual Studio linker tends to run out of memory when linking clang
or clang tools. You will want to use the 64-bit hosted Visual Studio toolset
instead, which will require a 64-bit version of Windows too.

Prerequisites:

- Visual Studio 2022, Python (version 3.7), and versions of UNIX
command-line tools.

- Install UNIX command-line tools via
[Cygwin](https://www.cygwin.com/).   Install the `Base` package.
Add the `bin` directory in your `Cygwin` install directory to to your system path.

- If Ninja is not already installed on your machine, you can download it from
[here](https://github.com/ninja-build/ninja/releases). Remember to set path to
the ninja executable.

In order to limit the amount of build parallelism with Visual Studio:
- Debug->Options->Projects and Solutions->VC++ Project Settings
- Set `Maximum Number of concurrent C++ compilations` to 3, if your development
machine has 1 GByte of memory or more per core. If not, see the
[Wiki page](https://github.com/checkedc/checkedc-clang/wiki/Parallel-builds-of-clang-on-Windows/)
to figure out what number to use. By default, 0 causes it to be the number of
available CPU cores on your machine, which is too much. You should also to go to
Debug->Options->Projects and Solutions->Build and Run and set the maximum number
of parallel project builds to be 3/4 of the actual number of CPU cores on your
machine.

A note about line endings:
LLVM/Clang have some tests that depend on using UNIX line ending conventions
(line feeds only).  This means that the sources you will be working with
need to end with line feeds. Visual Studio preserves line endings for files, so
this should work fine most of the time.  If you are creating a file, you will
need to save it using line feeds only
(go to File->Advanced Save Options to set this option before saving the file).
Otherwise, Visual Studio will save the file with carriage return/line feed line
endings.

## Source organization
LLVM uses Git for distributed source code control. It is mirrored by a Git
repository on Github: [llvm project](https://github.com/llvm/llvm-project)

The code for the Checked C version of LLVM/Clang lives in the following
repository: [Checked C clang repo](https://github.com/checkedc/checkedc-clang).
It is licensed under the
[University of Illinois/NCSAlicense](https://opensource.org/licenses/NCSA).
See the file LICENSE.TXT in for complete details of licensing.

The LLVM/Clang repo has two branches:

- master: the main development branch  for Checked C. All changes committed here
have been code reviewed and passed testing.
- baseline: these are pristine copies of the Github mirrors. Do not commit
changes for Checked C to the baseline branches.

There are tests in three locations: the
[Checked C repo](https://github.com/checkedc/checkedc), the
[Checked C clang repo](https://github.com/checkedc/checkedc-clang), and the
[Checked C LLVM Test Suite](http://github.com/checkedc/checkedc-llvm-test-suite).
The [Checked C repo](https://github.com/checkedc/checkedc) tests are language
conformance tests, so they are placed with the specification, not with the
compiler. The Checked C repo tests are licensed under the
[MIT license](https://opensource.org/licenses/MIT). The
[Checked C LLVM Test Suite](http://github.com/checkedc/checkedc-llvm-test-suite)
is a fork of the
[LLVM test suite mirror](https://github.com/llvm-mirror/test-suite). The LLVM
test suite is for extended testing and includes applications and benchmarks.
Some of these benchmarks have been modified to use Checked C extensions.

We do not recommend that developers install sources for it or the Checked C
version by default. The test suite is meant to be run as part of automated
integration testing or for changes that require extensive testing, not as part
of day-to-day development. For developers who need to install it, information is
[here](https://github.com/checkedc/checkedc-llvm-test-suite/blob/master/README.md).

## Checkout and Build Instructions for Checked C Compiler

You will need to choose a drive that has at least 50 Gbytes free. You may need
lots of space for the sources and the build. You can store the sources in any
directory that you want. You should avoid spaces in parent directory names
because this can confuse some tools.

### Instructions for Linux:

1. Choose any directory as your working directory. We will refer to to this
directory as \<WORK_DIR\>.

   ```
   cd <WORK_DIR>
   ```

2. Clone the `checkedc-clang` repository:

   ```
   git clone https://github.com/checkedc/checkedc-llvm-project src
   ```

3. The Checked C language header files and tests are stored in a folder within
`llvm/project`. Change to the  `src/llvm/projects/checkedc-wrapper` directory and clone the Checked C
repo:
   ```
   git clone https://github.com/checkedc/checkedc
   ```

4. **\[OPTIONAL\]** Install `ccache` to speed up the compiler build on Linux and
MacOS. [ ccache](https://ccache.samba.org) is a smart cache for GCC or Clang. It
works as a shim, and uses the hash of source files and their included headers
and build options to decide if an output needs recompiling, instead of file
modification time (which CMake uses). In some circumstances, this can cut
second-build (i.e. `ninja` where some of the files are already built) time down
from 5 minutes to 30 seconds. This still depends on how your header files and
includes are organized. Moreover, there are ways to share and control the size
of the cache directory, which is where `ccache` stores a copy of any object
files it has compiled.
   ```
   sudo apt install ccache
   ```

5. LLVM and Clang use `CMake`, which is a meta-build system generator. It
generates build systems for a specific platform. Create a build directory that
is a sibling of your LLVM source tree, like \<WORK_DIR\>/build.

   ```
   cd <WORK_DIR>/build
   ```

6. Execute the following `cmake` command in the build directory:

   ```
   cmake -G Ninja -DLLVM_ENABLE_PROJECTS=clang -DCMAKE_INSTALL_PREFIX=<WORK_DIR>/install -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON  -DLLVM_INSTALL_TOOLCHAIN_ONLY=ON  <WORK_DIR>/src/llvm
   ```
   Here is an explanation of the options:
   ```
   -DLLVM_ENABLE_PROJECTS=clang   // Required to enable Clang build
   -DCMAKE_INSTALL_PREFIX=<WORK_DIR>/install     // Directory where the compiler will be
                                                 // installed when "ninja install" is executed. 
   -DCMAKE_BUILD_TYPE=Release                    // Alternate values: Debug, RelWithDebInfo,
                                                 // MinSizeRel.
   -DLLVM_ENABLE_ASSERTIONS=ON                   // Alternate value: OFF.
   -DLLVM_INSTALL_TOOLCHAIN_ONLY=ON              // OPTIONAL. This definition is required to
                                                 // build a package for installation on other
                                                 // machines.
   -DLLVM_LIT_ARGS=-v                            // Arguments to pass to the test framework
   <WORK_DIR>/src/llvm
   ```
   Here are some options that you can add.  On Linux, you can use ccache to speed up the build:
1. ```
   -DLLVM_CCACHE_BUILD=ON
    ```
   If you want to speed up build times, you can build a version of clang that only takes one architecture:
   ```
   -DLLVM_TARGETS_TO_BUILD="X86"
    ```

7. After executing the `cmake` command as above, build the compiler as follows:

   ```
   ninja            // This command will build the compiler and all other supporting tools.
   OR
   ninja clang      // This command will build only the compiler.

   ninja clean      // This command cleans the build directory.
   ```

### Instructions for Windows


1. Start a Visual Studio `x64 Native Tools Command Prompt`

2. Choose any directory as your working directory. We will refer to to this
directory as \<WORK_DIR\>.  In your Visual Studio Command Prompt:

   ```
   cd <WORK_DIR>
   ```

2. Clone the `checkedc-clang` repository: If you already have
`core.autocrlf=false` set for your global Git configuration, you can follow the
Unix/Linux directions. Otherwise, follow these directions:

   ```
   git clone -c core.autocrlf=false https://github.com/checkedc/checkedc-llvm-project src
   ```

3. The Checked C language header files and tests are stored in a folder within `llvm\project`. 
Change to the `src\llvm\projects\checkedc-wrapper` directory and clone the Checked C
repo:

   ```
   git clone https://github.com/checkedc/checkedc
   ```

4. LLVM and Clang use CMake, which is a meta-build system generator. It
generates build systems for a specific platform. Create a build directory that
is a sibling of your llvm source tree, like \<WORK_DIR\>\build. Be sure to
exclude the build directory from anti-virus scanning. On Windows 10, go to
`Settings->Update & Security->Windows Defender->Add an exclusion`.

      ```
      cd <WORK_DIR>\build
      ```

5. Execute the following `cmake` command:
```
   cmake -G Ninja -DLLVM_ENABLE_PROJECTS=clang -DCMAKE_INSTALL_PREFIX=<WORK_DIR>/install -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON  -DLLVM_INSTALL_TOOLCHAIN_ONLY=ON  <WORK_DIR>\srcllvm
```
For additional options, see the instructions for Linux.

6. After executing the `cmake` command as above, build the compiler as follows
(in the same command shell as above):

   ```
   ninja            // This command will build the compiler and all other supporting tools.
   OR
   ninja clang      // This command will build only the compiler.

   ninja clean      // This command cleans the build directory.
   ```

### Instruction for Windows (Visual Studio)

On Windows, you can use Visual Studio for developemt.  It has support
for navigating the code base, code browsing, Intellisense,
and symbolic debugging.

#### Visual Studio
After you have followed the earlier instructions to set up the build system:
- Start Visual Studio->Open a Local Folder.
- To build llvm and clang: Open the src\llvm directory (because this contains
the llvm CMakeLists file).
- To build only clang: Open the src\clang directory (because this contains the
clang CMakeLists file).

To configure:
- Project->Generate Cache for LLVM

To add options to CMake:
- Project->CMake Settings for LLVM
- Add your options to "Cmake command arguments" and save
- Project->CMake Cache->Delete Cache, and configure again

To build:
- Build->Build All

The above instructions would build an X64 Release version of clang and the
default build dir is build dir is src/llvm/out. You can change this (and many
other settings) from Project->CMake Settings for LLVM.

To build an X86 version of clang:
- Project->CMake Settings for LLVM
- Toolset->msvc_x86_x64


## Testing

See the [Testing](Testing.md) page for directions on how to test the compiler
once you have built it.  We are testing the Checked C version of clang on x86
and x64 Windows and on x64 Linux.

## Building an LLVM package.

If you would like to build an LLVM package, first follow the steps in setting
up a build directory for building a package. On both Windows and Linux, change
directory to the build directory, and run

 `ninja package`.

## Updating sources to the latest sources for LLVM/Clang

Most developers can ignore this section. We periodically update the Checked C
source code to newer versions of the source code for LLVM/Clang. The directions
for the process of updating the baseline and master branches to newer versions
of LLVM/Clang are [here](Update-to-latest-LLVM-sources.md).
