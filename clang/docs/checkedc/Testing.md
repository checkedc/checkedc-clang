# Testing the Checked C version of LLVM/clang

LLVM/clang have two kinds of tests: developer regression tests and extended
tests.  Developer regression tests are meant to be run by developers before any
check-in and are quick to run.  Extended tests are run as part of continuous
integration testing or for changes that require extensive testing.

## Developer regression tests

We have created a new set of developer regression tests for the Checked C extension.
We have added a new target to the build system for running the test suite: check-checkedc.
The Checked C version of clang/LLVM should pass the existing clang and LLVM
regression tests and the Checked C-specific regression tests.   There should
be no unexpected test failures.  A developer should confirm this before committing a change.

When testing a change, the testing should be sufficient for the type of change.  For changes
to parsing and typechecking, it is usually sufficient to pass the Checked C and clang tests.

## Running developer regressions tests

### From Visual Studio
Load the solution and the open it using the Solution explorer (View->Solution Explorer).  To run tests, you can right click and build the following targets:

- Checked C tests: go to _CheckedC tests->check-checkedc_
- clang tests: go to _Clang tests->check-clang_
- All LLVM and clang tests: select the check-all solution (at the top level)

### From a command shell using msbuild
Set up the build system and then change to your new object directory.  Use the following commands to run tests:

- Checked C tests: `msbuild projects\checkedc-wrapper\check-checkedc.vcxproj /p:CL_MPCount=3 /m`
- Clang tests: `msbuild tools\clang\test\check-clang.vcxproj /p:CL_MPCount=3 /m`
- All LLVM and clang tests: `msbuild check-all.vcxproj /p:CL_MPCount=3 /m`

### Using make
In your build directory,

- Checked C tests: `make -j nnn check-checkedc`
- clang tests: `make -j nnn check-clang`
- All tests: `make -j nnn check-all`

where `nnn` is replaced by the number of CPU cores that your computer has.

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

## Extended testing

The extended testing for LLVM/clang is somewhat complicated to set up.  It
is oriented toward automated testing using servers and has the set-up complexity
that comes with that. For the Checked C implementation, there are two kinds
of extended testing that we do: testing that benchmarks that have been converted
to Checked C work and testing that the Checked C implementation has not broken existing functionality.

To run benchmarks, clone the
[Checked C LLVM test suite repo](https://github.com/microsoft/checkedc-llvm-test-suite)
and follow the directions in the
[README.md file](https://github.com/Microsoft/checkedc-llvm-test-suite/blob/master/README.md).
To test that existing functionality has not been broken,
check out the `original` branch of the
[Checked C LLVM test suite repo](https://github.com/microsoft/checkedc-llvm-test-suite)
and then follow the directions in the
[README.md file](https://github.com/Microsoft/checkedc-llvm-test-suite/blob/master/README.md).
Note that the  `original` branch contains tests as well as benchmarks.
It is much larger than the `master` branch.  It will use more than 2 GBytes of disk space
and the testing will take much longer to run.


## Gotchas

Windows x86 will cause issues if executables have "install", "setup", or "update" in
their filename. It will assume the executable is an installer, and require that the user has
elevated privileges to run the program. Usually inside visual studio this manifests itself
as "WinError 740". There is more documentation on the heuristics used here:
https://msdn.microsoft.com/en-us/enus/library/aa905330.aspx

We originally diagnosed this issue thanks to this stackoverflow post:
http://stackoverflow.com/q/11573444

We ran into this issue when compiling and running regression tests to ensure that dynamic
checking was being performed correctly.
