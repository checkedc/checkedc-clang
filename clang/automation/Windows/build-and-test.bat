
@rem  Set up sources, build clang, and test clang.
@rem
@rem  This script assumes the following environment varibles have been set
@rem  before it is invoked.
@rem
@rem  BUILD_SOURCESDIRECTORY: the directory where the LLVM source will be
@rem                          placed.
@rem  BUILD_BINARIESDIRECTORY: the directory where the object directory will
@rem                            be placed.
@rem  BUILDCONFIGURATION: The clang version to build.  Must be one of Debug, 
@rem                      Release,ReleaseWithDebInfo.
@rem  TEST_SUITE: the test suite to run.  Must be one of:
@rem              - CheckedC: run the Checked C regression tests.
@rem              - CheckedC: run the Checked C and clang regression tets.
@rem              - CheckedC_LLVM: run the Checked C and LLVM regression tests
@rem               
@rem
@rem  The following variable may be optionally set:
@rem  BUILDOS: The OS that we building upon.  May be one of Windows or WSL.
@rem           WSL stands for Windows Subsystem for Linux.  Defaults to
@rem           X86.
@rem  BUILD_PACKAGE: Build an installation package.  May be one of Yes or No.
@rem                 Defaults to No.  If this is Yes and the build is a Release
@rem                 build, assertions are enabled during the build.
@rem  SIGN_INSTALLER: Sign the installer package.
@rem  TEST_TARGET_ARCH: the target architecuture on which testing will be
@rem                    run.  May be one of X86 or AMD64.  Defaults to X86.
@rem
@rem  Because the Checked C clang build involves 3 repositories that may
@rem  be in varying states of consistency, the following variables can
@rem  optionally be set to specify branches or commits to use for testing.
@rem
@rem  CHECKEDC_BRANCH: defaults to master
@rem  CHECKEDC_COMMIT: defaults to HEAD
@rem  CLANG_BRANCH: If not set, uses BUILD_SOURCEBRANCHNAME if defined.
@rem                If BUILD_SOURCEBRANCHNAME is not defined, defaults
@rem                 to master
@rem  CLANG_COMMIT: If not set, uses BUILD_SOURCEVERSION if defined.
@rem                If BUILD_SOURCEVERSION is not default, defaults to
@rem                HEAD.
@rem  SIGN_BRANCH: signing automation branch to checkout.

@setlocal
@set DIRNAME=%CD%
@call %DIRNAME%\config-vars.bat
if ERRORLEVEL 1 (goto cmdfailed)

echo.
echo.Setting up files.

call %DIRNAME%\setup-files.bat
if ERRORLEVEL 1 (goto cmdfailed)

echo.
echo.Running cmake

call %DIRNAME%\run-cmake.bat
if ERRORLEVEL 1 (goto cmdfailed)

echo.
echo.Building and testing clang

call %DIRNAME%\test-clang.bat
if ERRORLEVEL 1 (goto cmdfailed)

call %DIRNAME%\build-package.bat
if ERRORLEVEL 1 (goto cmdfailed)

:succeeded
  cd %OLD_DIR%
  exit /b 0

:cmdfailed
  echo.Build or testing failed.
  cd %OLD_DIR%
  exit /b 1

