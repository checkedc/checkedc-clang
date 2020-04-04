# Setting up your machine and building clang

Note: The automation scripts used to build and test the Checked C compiler have
now been moved to their own [repo](https://github.com/microsoft/checkedc-automation).
For Windows builds we have deprecated msbuild and have now switched to using
CMake with Ninja.

## Setting up your machine

See the clang [Getting started guide](http://clang.llvm.org/get_started.html) for information
on how to set up your machine.

We now use CMake and Ninja for Clang builds on both Windows and Linux. For
Linux, install CMake 3.8 or later. For Windows, CMake is bundled as
part of your Visual Studio install.

### Developing on Windows

We recommend that you use a 64-bit version of Windows. We have found that the 32-bit hosted
Visual Studio linker tends to run out of memory when linking clang or clang tools.  You will
want to use the 64-bit hosted Visual Studio toolset instead, which will require a 64-bit version
of Windows too.
 
Prerequisites:

- Visual Studio 2017 or later, Python (version 2.7), and versions of UNIX
  command-line tools.  We recommend using Visual Studio 2019.
  - For VS2019, go to Tools -> Get Tools and Features (this opens the VS installer)
  - Go to Individual Components
  - Scroll to the “SDKs, libraries, and frameworks” section (near the bottom of the list)
  - Check “C++ ATL for latest v142 build tools (x86 and x64)”
  - Install

- For UNIX command-line tools, install them via [GnuWin32](https://sourceforge.net/projects/getgnuwin32/postdownload)
  - In cmd prompt, cd to the download dir and run:
  - download.bat
  - install.bat C:\GnuWin32
  - set PATH=C:\GnuWin32\bin;%PATH%

- If Ninja is not already available on your machine, you can download it from [here](https://github.com/ninja-build/ninja/releases).
  Remember to set path to the ninja executable.

In order to limit the amount of build parallelism with Visual Studio:
- Debug->Options->Projects and Solutions->VC++ Project Settings
- Set `Maximum Number of concurrent C++ compilations` to 3, if your development machine has
1 GByte of memory or more per core. If not, see the [Wiki page](https://github.com/Microsoft/checkedc-clang/wiki/Parallel-builds-of-clang-on-Windows/)
to figure out what number to use.
By default, 0 causes it to be the number of available CPU cores on your machine, which is too much.
You should also to go to  Debug->Options->Projects and Solutions->Build and Run and
set the maximum number of parallel project builds to be 3/4 of the actual number of CPU cores on
your machine.  

A note about line endings:
LLVM/Clang have some tests that depend on using UNIX line ending conventions
(line feeds only).  This means that the sources you will be working with
need to end with line feeds. Visual Studio preserves line endings for files, so
this should work fine most of the time.  If you are creating a file, you will
need to save it using line feeds only
(go to File->Advanced Save Options to set this option before saving the file).
Otherwise, Visual Studio will save the file with carriage return/line feed line endings.

## Source organization
LLVM uses Git for distributed source code control.   It is mirrored by a Git repository on Github:
[llvm project](https://github.com/llvm/llvm-project)

The code for the Checked C version of LLVM/Clang lives in the following repository:
[Checked C clang repo](https://github.com/Microsoft/checkedc-clang)
It is licensed under the [University of Illinois/NCSA
license](https://opensource.org/licenses/NCSA).  See the file LICENSE.TXT in
for complete details of licensing.

The LLVM/Clang repo has two branches:

- master: the main development branch  for Checked C.   All changes committed here have been code reviewed and passed testing.
- baseline: these are pristine copies of the Github mirrors.   Do not commit changes for Checked C to the baseline branches.

There are tests in three locations:
the [Checked C repo](https://github.com/Microsoft/checkedc),
the [Checked C clang repo](https://github.com/Microsoft/checkedc-clang), and
the [Checked C LLVM Test Suite](http://github.com/Microsft/checkedc-llvm-test-suite).
The [Checked C repo](https://github.com/Microsoft/checkedc) tests are language conformance tests,
so they are placed with the specification, not with the compiler. The Checked C repo
tests are licensed under the [MIT license](https://opensource.org/licenses/MIT).
The [Checked C LLVM Test Suite](http://github.com/Microsft/checkedc-llvm-test-suite) is a fork
of the [LLVM test suite mirror](https://github.com/llvm-mirror/test-suite).
It will contain benchmarks that have been modified to use Checked C extensions.
The LLVM test suite is for extended testing and includes applications and benchmarks.
We do not recommend that developers install sources for it or the
Checked C version by default.   The test suite is meant to be run as part of automated
integration testing or for changes that require extensive testing, not
as part of day-to-day development.
For developers who need to install it, information is
[here](https://github.com/Microsoft/checkedc-llvm-test-suite/blob/master/README.md).

## Setting up sources for development

You will need to choose a drive that has at least 50 Gbytes free.  You may need lots of space for the sources and the build.
You can store the sources in any directory that you want.  You should avoid spaces in parent directory names because this can confuse some tools.

The cloning process for LLVM/Clang depends on whether you are developing on
Unix/Linux or Windows.  LLVM/Clang have some tests that depend on using
UNIX line endings.  On Windows, Git can alter line endings to match the
Windows line ending convention.  It is important to
prevent Git from altering the line endings.

### Cloning LLVM/Clang on Unix/Linux

Clone the following repo to your desired location on your machine:
```
git clone https://github.com/Microsoft/checkedc-clang src
```
The Checked C language tests live in a folder within `llvm/project`.  Change to the `src/llvm/projects/checkedc-wrapper` directory
and clone the Checked C repo:
```
git clone https://github.com/Microsoft/checkedc
```

### Cloning LLVM/Clang on Windows

If you already have `core.autocrlf=false` set for your global Git
configuration, you can follow the Unix/Linux directions.
Otherwise, follow these directions:
```
git clone -c core.autocrlf=false https://github.com/Microsoft/checkedc-clang src
```

The Checked C language tests live in a folder within `llvm\project`.  Change to the `src\llvm\projects\checkedc-wrapper` directory
and clone the Checked C repo:
```
git clone https://github.com/Microsoft/checkedc
```

## Setting up a build directory

1. LLVM and Clang use CMake, which is a meta-build system generator. It generates build systems for a specific platform.
2. Create a build directory that is a sibling of your llvm source tree.  For example, if your sources are in MyDir\src, create MyDir\build.      
3. Be sure to exclude the build directory from anti-virus scanning.   On Windows 10, go to Settings->Update & Security->Windows Defender->Add an exclusion.
4. CMake will produce a build system by default that builds code generators for all LLVM-supported architectures.
   This can increase build and link times.  You might want to build the code generator for a specific target, such as x86.  To
   do that,  add `-DLLVM_TARGETS_TO_BUILD="X86"` to the command-line below.
4. Make sure to set the following CMake flag to enable clang in your builds: -DLLVM_ENABLE_PROJECTS=clang
5. Make sure that you are using whatever shell you normally do compiles in.

On Windows and Linux, cd your build directory and invoke CMake:
```
  cmake -G Ninja {llvm-path}
```
where `{llvm-path}` is the path to the root of your LLVM repo.

### Building an LLVM package (advanced topic)
If you are just trying out Checked C, you can safely ignore this section.  If
you plan to build an LLVM package for installation on other machines,  we
recommend that you build a release build of clang with assertions on and only
include the toolchain in
the package.  On Windows, you can add the following flags to your CMake options:
```
  -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_INSTALL_TOOLCHAIN_ONLY=ON -DLLVM_USE_CRT_RELEASE=MT
```
On UNIX you can add,
```
  -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_INSTALL_TOOLCHAIN_ONLY=ON
```

## Building

Note that the first time that you build clang, it may take over an hour to
build.  This is because LLVM is being built.   The debug build of LLVM is
particularly slow because it bottlenecks on table generation. LLVM generates
architecture-specific tables at build time that are used during code
generation.  The default table generation algorithm is very slow in debug
builds.  Subsequent builds during development will be much faster (minutes, not
an hour).

### On UNIX
Change to your build directory and build `clang`:
```
  ninja clang
```

### On Windows

For day-to-day development, we recommend building from Visual Studio.  This
will improve your productivity significantly because it will give you all the
capabilities of Visual Studio for navigating the code base, code browsing, and
Intellisense.

#### Visual Studio
After you have followed the earlier instructions to set up the build system:
- Start Visual Studio->Open a Local Folder.
- To build llvm and clang: Open the src/llvm directory (because this contains the llvm CMakeLists file).
- To build only clang: Open the src/clang directory (because this contains the clang CMakeLists file).

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

#### Command-shell

Follow the earlier instructions to set up the build system.

To build X64 version of clang:
```
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
```

To build X86 version of clang:
```
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x86
```

From the build directory, use the following command to build clang only:
```
  ninja clang
```

To build everything:
```
  ninja
```

To clean the build directory:
```
  ninja clean
```

## Testing

See the [Testing](Testing.md) page for directions on how to test the compiler once you have built it.  We
are testing the Checked C version of clang on x86 and x64 Windows and on x64 Linux.

## Building an LLVM package.

If you would like to build an LLVM package, first follow the steps in setting up a build directory for
building a package. On both Windows and Linux, change directory to the build directory, and run
  ninja package

## Updating sources to the latest sources for LLVM/Clang

Most developers can ignore this section. We periodically update the Checked C source code
to newer versions of the source code for LLVM/Clang.  The directions for the process of updating the
baseline and master branches to newer versions of LLVM/Clang are
[here](Update-to-latest-LLVM-sources.md).

## Tips for a faster build on Linux/Mac OS X

[ccache](https://ccache.samba.org) is a smart cache for GCC or Clang. It works as a shim, and
uses the hash of source files and their included headers and build options to decide if an output
needs recompiling, instead of file modification time (which Make uses). In some circumstances,
this can cut second-build (i.e. `make` where some of the files are already built) time down
from 5 minutes to 30 seconds. This still depends on how your header files and includes are organised.

To make your LLVM/Clang builds get this speedup, install ccache (packages available for most systems,
on Mac OS X it's in Homebrew), then run cmake with `LLVM_CCACHE_BUILD=On`. There are ways to share and
control the size of the cache directory, which is where ccache stores a copy of any object files
it has compiled.
