@echo off
rem Create configuration variables

if "%BUILDCONFIGURATION%"=="Debug" (
  rem
) else if "%BUILDCONFIGURATION%"=="Release" (
  rem
) else if "%BUILDCONFIGURATION%"=="ReleaseWithDebInfo" (
  rem
) else (
  echo Unknown BUILDCONFIGURATION value "%BUILDCONFIGURATION%"
  exit /b 1
)

if "%BUILDOS%"=="Windows" (
  rem
) else if "%BUILDOS"=="WSL" (
  rem
) else (
  echo Unknown BUILDOS value "%BUILDOS%"
  exit /b 1;
)

set LLVM_OBJ_DIR=%BUILD_BINARIESDIRECTORY%\LLVM-%BUILDCONFIGURATION%-%BUILDOS%.obj

rem Validate Test Suite configuration

if "%TEST_SUITE%"=="CheckedC" (
  rem  c
) else if "%TEST_SUITE%"=="CheckedC_clang" (
  rem
) else if "%TEST_SUITE%"=="CheckedC_LLVM" (
  rem
) else (
  echo Unknown TEST_SUITE value "%TEST_SUITE%"
)

if "%MSBUILD_BIN%"=="" (
  set MSBUILD_BIN="%programfiles(x86)%\MSBuild\14.0\Bin\MSBuild.exe"
)

if not exist "%MSBUILD_BIN%" (
    echo Cannot find MSBuild in "%MSBUILD_BIN%"
    exit /b 1;
)

if "%NUMBER_OF_PROCESSORS%"=="" (
    set MSBUILD_CPU_COUNT=4
) else if %NUMBER_OF_PROCESSORS LSS 16 (
    set MSBUILD_CPU_COUNT=4
) else (
    set /a "MSBUILD_CPU_COUNT=%NUMBER_OF_PROCESSORS%/4"
)

echo Configured environment variables:
echo.
echo.  LLVM_OBJ_DIR: %LLVM_OBJ_DIR%
echo.  TEST_SUITE: %TEST_SUITE%
echo.  MSBUILD_CPU_COUNT: %MSBUILD_CPU_COUNT%