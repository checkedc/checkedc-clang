@echo off
rem Create configuration variables

if NOT DEFINED BUILDCONFIGURATION (
  echo BUILDCONFIGURATION must be defined
  exit /b 1
) else if "%BUILDCONFIGURATION%"=="Debug" (
  rem
) else if "%BUILDCONFIGURATION%"=="Release" (
  rem
) else if "%BUILDCONFIGURATION%"=="ReleaseWithDebInfo" (
  rem
) else (
  echo Unknown BUILDCONFIGURATION value %BUILDCONFIGURATION%
  exit /b 1
)

if NOT DEFINED BUILDOS (
  echo BUILDOS must be defined
  exit /b 1
) else if "%BUILDOS%"=="Windows" (
  rem
) else if "%BUILDOS%"=="WSL" (
  rem
) else (
  echo Unknown BUILDOS value %BUILDOS%
  exit /b 1;
)

if NOT DEFINED TEST_TARGET_ARCH (
  set TEST_TARGET_ARCH=X86
) else if "%TEST_TARGET_ARCH%"=="X86"  (
  rem
) else if "%TEST_TARGET_ARCH%"=="AMD64"  (
  rem
) else (
  echo Unknown TEST_TARGET_ARCH value %TEST_TARGET_ARCH
  exit /b 1;
)

set LLVM_OBJ_DIR=%BUILD_BINARIESDIRECTORY%\LLVM-%BUILDCONFIGURATION%-%TEST_TARGET_ARCH%-%BUILDOS%.obj

rem Validate Test Suite configuration

if NOT DEFINED TEST_SUITE (
  echo TEST_SUITE must be defined
  exit /b 1
) else if "%TEST_SUITE%"=="CheckedC" (
  rem  c
) else if "%TEST_SUITE%"=="CheckedC_clang" (
  rem
) else if "%TEST_SUITE%"=="CheckedC_LLVM" (
  rem
) else (
  echo Unknown TEST_SUITE value %TEST_SUITE%
  exit /b 1
)

rem set up branch names
if not defined %LLVM_BRANCH% (
  set LLVM_BRANCH=master
) else if "%LLVM_BRANCH%"=="" (
  set LLVM_BRANCH=master
)

if not defined %CHECKEDC_BRANCH% (
  set CHECKEDC_BRANCH=master
) else if "%CHECKEDC_BRANCH%"=="" (
  set CHECKEDC_BRANCH=master
)

if not defined %CLANG_BRANCH% (
  set CLANG_BRANCH=%BUILD_SOURCEBRANCHNAME%
) else if "%CLANG_BRANCH%"=="" (
  set CLANG_BRANCH=master
)

rem set up source versions (Git commit number)
if not defined %LLVM_COMMIT% (
  set LLVM_COMMIT=HEAD
)

if not defined %CHECKEDC_COMMIT% (
  set CHECKEDC_COMMIT=HEAD
)

if not defined %CLANG_COMMIT% (
  set CLANG_COMMIT=HEAD
) else (
  set CLANG_COMMIT=%BUILD_SOURCEVERSION%
)

if NOT DEFINED MSBUILD_BIN (
  set "MSBUILD_BIN=%programfiles(x86)%\MSBuild\14.0\Bin\MSBuild.exe"
)

if NOT DEFINED %NUMBER_OF_PROCESSORS% (
  set MSBUILD_CPU_COUNT=4
) else if %NUMBER_OF_PROCESSORS% LSS 16 (
  set MSBUILD_CPU_COUNT=4
) else (
  set /a "MSBUILD_CPU_COUNT=%NUMBER_OF_PROCESSORS%/4"
)

echo Configured environment variables:
echo.
echo.  LLVM_OBJ_DIR: %LLVM_OBJ_DIR%
echo.  TEST_TARGET_ARCH: %TEST_TARGET_ARCH%
echo.  TEST_SUITE: %TEST_SUITE%
echo.  MSBUILD_BIN: %MSBUILD_BIN%
echo.  MSBUILD_CPU_COUNT: %MSBUILD_CPU_COUNT%
echo.  Branch and commit information:
echo.    CLANG_BRANCH: %CLANG_BRANCH%
echo.    CLANG_COMMIT: %CLANG_COMMIT%
echo.    LLVM_BRANCH: %LLVM_BRANCH%
echo.    LLVM_COMMIT: %LLVM_COMMIT%
echo.    CHECKEDC BRANCH: %CHECKEDC_BRANCH%
echo.    CHECKEDC_COMMIT: %CHECKEDC_COMMIT%
