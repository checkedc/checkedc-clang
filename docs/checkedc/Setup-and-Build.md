# Setting up your machine and building clang

## Setting up your machine

See the clang [Getting started guide](http://clang.llvm.org/get_started.html) for information
on how to set up your machine.

### Developing on Windows

We are doing the development work for Checked C on Windows. We have a few recommendation for developing 
clang on Windows using Visual Studio. 
 
You will need to install the following before building: 

- Visual Studio 2013 or later, cmake, Python (version 2.7), and versions of UNIX command line tools. We
recommend using Visual Studio 2015. 
- For UNIX command-line tools, we recommend installing them via Cygwin because these are well-maintained. 
Go to [http://www.cygwin.com](http://www.cygwin.com) and download the installer(put it in a known place).
Then run it and use the GUI to install the coreutils and diffutils packages.  Add the bin subdirectory to your system path.

If you plan to use Visual Studio to build projects, lower the default number of projects that will be built in parallel. 
The Visual Studio solution for clang has lots of parallelism. The parallelism can cause your machine to use too much
virtual memory and become unresponsive when building.  In VS 2015, go to _Debug->Options->Projects and Solutions ->Build and Run_ and 
set the maximum number of parallel project builds to a fraction of the actual number processors
(for example, number of processor cores on your machine/3).

## Source organization
LLVM uses subversion for distributed source code control.   It is mirrored by Git repositories on Github: the
[LLVM mirror](https://github.com/llvm-mirror/llvm) and
[clang mirror](https://github.com/llvm-mirror/clang).

The code for the Checked C version of LLVM/clang lives in two repositories: the [Checked C clang repo](https://github.com/Microsoft/checked-clang)
and the [Checked C LLVM repo](https://github.com/Microsoft/checkedc-llvm).  Each repo is licensed 
under the [University of Illinois/NCSA license](https://opensource.org/licenses/NCSA).
See the file LICENSE.TXT in either of the repos for complete details of licensing.  

The tests live in the [Checked C repo](https://github.com/Microsoft/checkedc).These are
language conformance tests, so they are placed with the specification, not with the compiler.
The test code is licensed under the [MIT license](https://opensource.org/licenses/MIT).  

The clang and LLVM repos have two branches:

- master: the main development branch  for Checked C.   All changes committed here have been code reviewed and passed testing.
- baseline: these are pristine copies of the Github mirrors.   Do not commit changes for Checked C to the baseline branches.

Follow the directions here to update the baseline branches to newer versions of the LLVM/clang sources.

## Setting up sources for development

You will need to choose a drive that has at least 20 Gbytes free.  You may need lots of space for the sources and the build.   
You can store the sources in any directory that you want.  You should avoid spaces in parent directory names because this can confuse some tools.

You will need to clone each repo and checkout the master branch from each repo.  First clone LLVM to your desired location on your machine:
```
git clone https://github.com/Microsoft/checkedc-llvm
git checkout master
```
Clang  needs to be placed in the tools subdirectory of LLVM.  Change to the llvm\tools directory and clone the clang repo:
```
git clone https://github.com/Microsoft/checkedc-clang
git checkout master
```
The Checked C language tests live in a project directory for LLVM.  Change to the  llvm\projects\checkedc-wrapper directory
and clone the Checked C repo:
```
git clone https://github.com/Microsoft/checkedc
git checkout master
```

## Setting up a build directory

1. LLVM and clang use cmake, which a meta-build system generator. It generates build systems for a specific platform. 
2. Create a build directory that is a sibling of your llvm source tree.  For example, if llvm is in MyDir\llvm, create MyDir\llvm.obj.      
3. Be sure to exclude the build directory from anti-virus scanning.   On Windows 10, go to Settings->Update & Security->Windows Defender->Add an exclusion.
4. You may optionally want to create an install directory, which is the place where an installed version of LLVM can be placed. 
An install directory is different than the build directory, which will contain the results of building LLVM.
5. Cmake will produce a build system by default that builds all LLVM target.  This can lead to slow build and link times.  We are developing and testing
   Checked C on x86 right now, so we recommend only building only that target.  None of the changes made for Checked C are target-specific, so other targets such as
   x64 should work too.  Use the command-line in the next item to avoid building all targets.
6. Make sure that you are using whatever shell you normally do compiles in.  Cd to your build directory and invoke cmake with: 

	cmake -DLLVM\_TARGETS\_TO\_BUILD="X86" -DCMAKE\_INSTALL\_PREFIX=_path to directory to install in_  _llvm-path_
where llvm-path is the path to the root of your LLVM repo.
	
## Building

You can build `clang` the usual way that it is built.   The earlier build system directions will create a Debug build,
so `clang` will be replaced in your build directory under `Debug\bin`.

### On Unix

Change to your build directory and just run `make`:

	make

For subsequent builds, you can just build `clang`:

	make clang

### On Windows

For day-to-day development, we recommend building from Visual Studio.  This will improve your productivity significantly because it will give you
all the capabilities of Visual Studio for navigating the code base, code browsing, and Intellisense.  Note that VS launches a multi-threaded build 
by default.  Be sure you have throttled down the number of processes following earlier directions. 

#### Visual Studio
Follow the earlier instructions to set up the buld system.  After you've done that, there should be a solution file LLVM.sln in
your build directory.  Use Visual Studio to load the solution file. Then open the solution explorer (under View->Solution Explorer). 

To build

- clang only: go to _clang executables directory -> clang_ and right click to build `clang'.
- Everything: right click on the solution and select build.

#### Command-shell

Follow the earlier instruction to set up the build system.  Form the build directory, use the following comamnd to build clang only:

	msbuild tools\clang\tools\driver\clang.vcxproj /maxcpucount:number of processors/3

To build everything:

	msbuild LLVM.sln //maxcpucount:number of processors/3

To clean the build directory:

	msbuild /t:clean LLVM.sln
