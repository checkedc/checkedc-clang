# Setting up your machine and building clang

## Setting up your machine

See the clang [Getting started guide](http://clang.llvm.org/get_started.html) for information
on how to set up your machine. If you will be developing on Windows, you should install CMake 3.8
or later on your machine.

### Developing on Windows

We are doing the development work for Checked C on Windows. We have a few recommendations for developing 
clang on Windows using Visual Studio.

We recommend that you use a 64-bit version of Windows. We have found that the 32-bit hosted
Visual Studio linker tends to run out of memory when linking clang or clang tools.  You will
want to use the 64-bit hosted Visual Studio toolset instead, which will require a 64-bit version
of Windows too.
 
You will need to install the following before building: 

- Visual Studio 2015 or later, CMake (version 3.8 or later), Python (version 2.7), and versions of UNIX command
line tools.  We recommend using Visual Studio 2017.
- For UNIX command-line tools, we recommend installing them via Cygwin because these are well-maintained. 
Go to [http://www.cygwin.com](http://www.cygwin.com) and download the installer (put it in a known place).
Then run it and use the GUI to install the coreutils and diffutils packages.  Add the bin subdirectory to your system path.

If you plan to use Visual Studio to build projects,  you must limit the amount
of parallelism that will be used during builds.  By default, the Visual Studio solution
for clang has [too much parallelism](https:/github.com/Microsoft/checkedc-clang/issues/268). 
The parallelism will cause your build to use too much physical memory and cause your machine
to start paging.  This will make your machine unresponsive and slow down your build too.
See the Wiki page on [Parallel builds of clang on Windows](https://github.com/Microsoft/checkedc-clang/wiki/Parallel-builds-of-clang-on-Windows/)
for more details.

in VS 2017, go to _Debug->Options->Projects and Solutions->VC++ Project Settings_ and set
the `Maximum Number of concurrent C++ compilations` to 3, if your development machine has
1 GByte of memory or more per core.  If not, see the
[Wiki page](https://github.com/Microsoft/checkedc-clang/wiki/Parallel-builds-of-clang-on-Windows/)
to figure out what number to use.
By default, 0 causes it to be the number of available CPU cores on your machine, which is too much.
You should also to go to  _Debug->Options->Projects and Solutions->Build and Run_ and
set the maximum number of parallel project builds to be 3/4 of the actual number of CPU cores on
your machine.  

LLVM/clang have some tests that depend on using UNIX line ending conventions
(line feeds only).  This means that the sources you will be working with
need to end with line feeds. Visual Studio preserves line endings for files, so
this should work fine most of the time.  If you are creating a file, you will
need to save it using line feeds only
(go to File->Advanced Save Options to set this option before saving the file).
Otherwise, Visual Studio will save the file with carriage return/line feed line endings.

## Source organization
LLVM uses subversion for distributed source code control.   It is mirrored by Git repositories on Github: the
[LLVM mirror](https://github.com/llvm-mirror/llvm) and
[clang mirror](https://github.com/llvm-mirror/clang).

The code for the Checked C version of LLVM/clang lives in two repositories: the
[Checked C clang repo](https://github.com/Microsoft/checkedc-clang)
and the [Checked C LLVM repo](https://github.com/Microsoft/checkedc-llvm).  Each repo is licensed 
under the [University of Illinois/NCSA license](https://opensource.org/licenses/NCSA).
See the file LICENSE.TXT in either of the repos for complete details of licensing.  

The clang and LLVM repos have two branches:

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

checkedc-llvm contains two other repos as submodules: checkedc and checkedc-clang. Both are required to build successfully. We do not recommend using ```git clone```'s `recursive` option; this may give you stale versions of checkedc and checkedc-clang repos. Instead use git clone on each repository as specified below.

You will need to clone each repo.
The cloning process for LLVM and clang depends on whether you are developing on
Unix/Linux or Windows.  LLVM and clang have some tests that depend on using
UNIX line endings.  On Windows, Git can alter line endings to match the
Windows line ending convention.  It is important to
prevent Git from altering the line endings.

### Cloning LLVM/clang on Unix/Linux

First clone LLVM to your desired location on your machine:
```
git clone https://github.com/Microsoft/checkedc-llvm llvm
```
Clang  needs to be placed in the tools subdirectory of LLVM.  Change to the
`llvm\tools` directory and clone the clang repo:
```
git clone https://github.com/Microsoft/checkedc-clang clang
```
The Checked C language tests live in a folder within `\project`.  Change to the `llvm\projects\checkedc-wrapper` directory
and clone the Checked C repo:
```
git clone https://github.com/Microsoft/checkedc
```

### Cloning LLVM/clang on Windows

If you already have `core.autocrlf=false` set for your global Git
configuration, you can follow the Unix/Linux directions.
Otherwise, follow these directions:
```
git clone -c core.autocrlf=false https://github.com/Microsoft/checkedc-llvm llvm
```
Clang  needs to be placed in the tools subdirectory of LLVM.  Change to the `llvm\tools` directory and clone the clang repo:
```
git clone -c core.autocrlf=false https://github.com/Microsoft/checkedc-clang clang
```
The Checked C language tests live in a folder within `\project`.  Change to the `llvm\projects\checkedc-wrapper` directory
and clone the Checked C repo:
```
git clone https://github.com/Microsoft/checkedc
```

## Setting up a build directory

1. LLVM and clang use CMake, which is a meta-build system generator. It generates build systems for a specific platform.
2. Create a build directory that is a sibling of your llvm source tree.  For example, if llvm is in MyDir\llvm, create MyDir\llvm.obj.      
3. Be sure to exclude the build directory from anti-virus scanning.   On Windows 10, go to Settings->Update & Security->Windows Defender->Add an exclusion.
3. Cmake will produce a build system by default that builds code generators for all LLVM-supported architectures.
   This can increase build and link times.  You might want to build the code generator for a specific target, such as x86.  To
   do that,  add `-DLLVM_TARGETS_TO_BUILD="X86"` to the command-line below.
4. Make sure that you are using whatever shell you normally do compiles in.  On Linux, cd your build directory and invoke CMake
   with:
```
    cmake {llvm-path}
```
where `{llvm-path}` is the path to the root of your LLVM repo.

   On Windows, when using Visual Studio, you should specify that the 64-bit hosted toolset be used.
   Visual Studio has both 32-bit hosted and 64-bit hosted versions of tools.
   You can do that by adding the option `-T "host=x64"` to the command-line (note that this
   option is only available using CMake version 3.8 or later).
```
    cmake -T "host=x64" {llvm-path}
```

   On Windows, when using Visual Studio, CMake by default produces a build system for x86.  This means that
   the clang tests will run in 32-bit compatiblity mode, even on a 64-bit version of Windows.  To build and run
   tests on x64, specify a different generator using the `-G` option.  For Visual Studio 2017, you can use:
```
    cmake -T "host=x64" -G "Visual Studio 15 2017 Win64" {llvm-path}
```
`cmake --help` will list all the available generators on your platform.

### Building an LLVM package (advanced topic)
If you are just trying out Checked C, you can safely ignore this section.  If you plan to build an LLVM package for installation
on other machines,  we recommend that you build a release build of clang with assertions on and only include the toolchain in
the package.  On Windows, you can add the following flags to your cmake command line:
```
   -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_INSTALL_TOOLCHAIN_ONLY=ON -DLLVM_USE_CRT_RELEASE=MT
```
On UNIX you can add,
```
   -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_INSTALL_TOOLCHAIN_ONLY=ON
```

## Building

You can build `clang` the usual way that it is built.   The earlier build system directions will create a Debug build,
so `clang` will be placed in your build directory under `Debug\bin`.

Note that the first time that you build clang, it may take over an hour to build.  This is because LLVM is being
built.   The debug build of LLVM is particularly slow because it bottlenecks on table generation. LLVM generates architecture-specific
tables at build time that are used during code generation.  The default table generation algorithm is very slow in debug builds.
Subsequent builds during development will be much faster (minutes, not an hour).

### On UNIX

Change to your build directory and build `clang`:

	make -j nnn clang

where `nnn` is replaced by the number of CPU cores that your computer has.

### On Windows

For day-to-day development, we recommend building from Visual Studio.  This will improve your productivity significantly because it will give you
all the capabilities of Visual Studio for navigating the code base, code browsing, and Intellisense.  Note that VS launches a multi-threaded build 
by default.  Be sure you have throttled down the number of processes following earlier directions. 

#### Visual Studio
Follow the earlier instructions to set up the build system.  After you've done that, there should be a solution file LLVM.sln in
your build directory.  Use Visual Studio to load the solution file. Then open the solution explorer (under View->Solution Explorer). 

To build

- clang only: go to _clang executables directory -> clang_ and right click to build `clang'.
- Everything: right click on the solution and select build.

By default, the build type will be a Debug build.  You can switch to another build configuration in VS 2017
by selecting the Build->Configuration Manager menu item and choosing a different solution configuration.

#### Command-shell

Follow the earlier instructions to set up the build system.  From the build directory, use the following comamnd to build clang only:

	msbuild tools\clang\tools\driver\clang.vcxproj /p:CL_MPCount=3 /m

To build everything:

	msbuild LLVM.sln /p:CL_MPCount=3 /m

To clean the build directory:

	msbuild /t:clean LLVM.sln

By default, the build type is a Debug build.   You can specify the build type by adding `/p:Configuration=nnn`
to the `msbuild` command line, where `nnn` is one of `Debug`, `Release`, or `RelWithDebInfo`.  For example, 
for a Release build, use:

    msbuild tools\clang\tools\driver\clang.vcxproj /p:Configuration=Release /p:CL_MPCount=3 /m

## Testing

See the [Testing](Testing.md) page for directions on how to test the compiler once you have built it.  We
are testing the Checked C version of clang on x86 and x64 Windows and on x64 Linux.

## Building an LLVM package.

If you would like to build an LLVM package, first follow the steps in setting up a build directory for
building a package.   On Windows, install [NSIS](http://nsis.sourceforge.net).  Change directory to your
build directory, and run

	msbuild PACKAGE.sln /p:Configuration=Release /p:CL_MPCount=3 /m

On UNIX, run

	make -j nnn package

where `nnn` is replaced by the number of CPU cores that your computer has.

## Updating sources to the latest sources for clang/LLVM

Most developers can ignore this section. We periodically update the Checked C source code
to newer versions of the source code for clang/LLVM.  The directions for the process of updating the
baseline and master branches to newer versions of LLVM/clang are
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
