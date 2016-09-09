# Testing the Checked C version of LLVM/clang

We have created a new LLVM test suite for the Checked C extension.  We have added a new
target to the build system for running the test suite: check-checkedc. 

The Checked C version of clang/LLVM should pass the tests that the baseline version
of clang/LLVM (in the baseline branch) and the Checked C specific tests.   There should
be no unexpected test failures.  A developer should confirm this before committing a change.

When testing a change, the testing should be sufficient for the type of change.  For changes
to parsing and typechecking, it is usually sufficient to pass the Checked C and clang tests.

## Running tests

### From Visual Studio
Load the solution and the open it using the Solution explorer (View->Solution Explorer).  To run tests, you can right click and build the following targets:

- Checked C tests: go to _CheckedC tests->check-checkedc_
- clang tests: go to _Clang tests->check-clang_
- All LLVM and clang tests: select the check-all solution (at the top level)

### From a command shell using msbuild
Set up the build system and then change to your new object directory.  Use the following commands to run tests:

- Checked C tests: `msbuild projects\checkedc-llvm\check-checkedc.vcxproj /maxcpucount:`_number of processors_/3
- Clang tests: `msbuild tools\clang\test\check-clang.vcxproj /maxcpucount:1number of processors/3
- All LLVM and clang tests: `msbuild check-all.vcxproj /maxcpucount:`number of processors/3`

### Using make
In your build directory,

- Checked C tests: `make check-checkedc`
- clang tests: `make check-clang`
- All tests: `make check-all`

### From a command shell using the testing harness
You can use the testing harness to run individual tests or sets of tests.
These directions are adapted from http://llvm.org/docs/TestingGuide.html#id11.
The clang-specific documentation on running tests appears to be out-of-date, so don't try to follow those directions.  

- The build creates a llvm-lit.py script that you can use to run regression tests by hand.
  It is placed in the same directory as your newly built clang executable.   Those are placed in {your build directory}\debug\bin. 
- Set the environment variable CLANG to the full path of your clang executable.  For example:

		set CLANG=d:\autobahn1\llvm.obj\Debug\bin\clang.exe

- You can point the llvm-lit script at a directory to run all tests or an individual test.  For example:

		llvm-lit d:\autobahn1\llvm\tools\clang\test

### Unexpected test failures and line endings

If you see unexpected test failures when working on Windows using unchanged
versions of the Checked C LLVM/clang repos, you should first check that you
have set your line end handling for Git properly, particularly if you see a
lot of failures.  LLVM and clang have some tests that depend on line endings.
Those tests assume that all lines end with line feeds (the Unix line ending
convention).

The LLVM/clang project uses Subversion for source code control, which does not
alter line endings.  We are using Git for source code control, which may alter
line endings on Windows, depending on how you have configured Git.  Git may
alter text line endings to be carriage return/line feed (the Windows line
ending convention).  It is important to ensure that Git does not do this for
your LLVM/clang repositories.

The configuration setting `core.autocrlf` should to be set to `false`. If you
followed the recommended [steps](Setup-and-Build.md) for cloning your Git repos,
it will be set to `false`.
