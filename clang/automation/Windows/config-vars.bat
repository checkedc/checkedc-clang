rem
rem Validate and set configuration variables.   Other scripts should only 
rem depend on variables printed at the end of this script.
rem
rem This script is run as part of automated build and test validation.  
rem It has extra checking so that it can be run manually as well. It validates
rem that environment variables set by the system have been are present. When
rem running it manually, the variables must be set by the user.

rem Create configuration variables

set MSBUILD_VERBOSITY=n

if NOT DEFINED BUILD_CHECKEDC_CLEAN (
  if DEFINED BUILD_CLEAN (
    set BUILD_CHECKEDC_CLEAN=Yes
  ) else (
    set BUILD_CHECKEDC_CLEAN=No
  )
) else (
  if "%BUILD_CHECKEDC_CLEAN%"=="Yes" (
    rem
  ) else if "%BUILD_CHECKEDC_CLEAN%"=="No" (
    rem
  ) else (
    echo Unknown BUILD_CHECKEDC_CLEAN value %BUILD_CHECKEDC_CLEAN%: must be one of Yes or No
    exit /b /1
  )
)

rem Validate build configuration

if NOT DEFINED BUILDCONFIGURATION (
  echo BUILDCONFIGURATION not set: must be set to set to one of Debug, Release, ReleaseWithDebInfo
  exit /b 1
) else if "%BUILDCONFIGURATION%"=="Debug" (
  rem
) else if "%BUILDCONFIGURATION%"=="Release" (
  rem
) else if "%BUILDCONFIGURATION%"=="ReleaseWithDebInfo" (
  rem
) else (
  echo Unknown BUILDCONFIGURATION value %BUILDCONFIGURATION%: must be one of Debug, Release, ReleaseWithDebInfo
  exit /b 1
)

rem Validate build OS

if NOT DEFINED BUILDOS (
  set BUILDOS=Windows
) else if "%BUILDOS%"=="Windows" (
  rem
) else if "%BUILDOS%"=="WSL" (
  rem
) else (
  echo Unknown BUILDOS value %BUILDOS%: must be Windows or WSL
  exit /b 1;
)

rem Validate or set target architecture for testing.

if NOT DEFINED TEST_TARGET_ARCH (
  set TEST_TARGET_ARCH=X86
) else if "%TEST_TARGET_ARCH%"=="X86"  (
  rem
) else if "%TEST_TARGET_ARCH%"=="AMD64"  (
  rem
) else (
  echo Unknown TEST_TARGET_ARCH value %TEST_TARGET_ARCH: must be X86 or AMD64
  exit /b 1;
)

if NOT DEFINED BUILD_PACKAGE (
  set BUILD_PACKAGE=No
) else (
  if "%BUILD_PACKAGE%"=="Yes" (
    rem
  ) else if "%BUILD_PACKAGE%"=="No" (
    rem
  ) else (
    echo Unknown BUILD_PACKAGE value %BUILD_PACKAGE%: must be one of Yes or No
    exit /b /1
  )
)

if NOT DEFINED SIGN_INSTALLER (
    set SIGN_INSTALLER=No
) else if "%SIGN_INSTALLER%"=="Test" (
    if "%BUILD_PACKAGE"=="No" (
      echo "BUILD_PACKAGE must be Yes when SIGN_INSTALLER is Test"
      exit /b /1
    )
) else if "%SIGN_INSTALLER%"=="Release" (
    if "%BUILD_PACKAGE"=="No" (
      echo "BUILD_PACKAGE must be Yes when SIGN_INSTALLER is Release"
      exit /b /1
    )
) else (
    echo Unknown SIGN_INSTALLER value %SIGN_INSTALLER%: must be one of Test or Release
    exit /b /1
  )
)

if not defined BUILD_BINARIESDIRECTORY (
  echo BUILD_BINARIESDIRECTORY not set.  Set it the directory that will contain the object directory.
  exit /b 1
)

if not defined BUILD_SOURCESDIRECTORY (
   echo BUILD_SOURCESDIRECTORY not set.  Set it the directory that will contain the sources directory
   exit /b 1
)

set LLVM_OBJ_DIR=%BUILD_BINARIESDIRECTORY%\LLVM-%BUILDCONFIGURATION%-%TEST_TARGET_ARCH%-%BUILDOS%.obj

rem Validate Test Suite configuration

if NOT DEFINED TEST_SUITE (
  echo TEST_SUITE not set: must be set to one of CheckedC, CheckedC_clang, or CheckedC_LLVM
  exit /b 1
) else if "%TEST_SUITE%"=="CheckedC" (
  rem
) else if "%TEST_SUITE%"=="CheckedC_clang" (
  rem
) else if "%TEST_SUITE%"=="CheckedC_LLVM" (
  rem
) else (
  echo Unknown TEST_SUITE value %TEST_SUITE%: must be one of CheckedC, CheckedC_clang, or CheckedC_LLVM
  exit /b 1
)

rem SKIP_CHECKEDC_TESTS controls whether to skip the Checked C repo tests
rem entirely. This is useful for building/testing a stock (unmodified)
rem version of clang/LLVM that does not support Checked C.

if NOT DEFINED SKIP_CHECKEDC_TESTS (
  set SKIP_CHECKEDC_TESTS=No
) else if "%SKIP_CHECKEDC_TESTS%"=="Yes" (
  rem
) else if "%SKIP_CHECKEDC_TESTS%"=="No" (
  rem
) else (
  echo Unknown SKIP_CHECKEDC_TESTS value: must be one of Yes or No
  exit /b 1
)

rem set up branch names
if not defined CHECKEDC_BRANCH (
  set CHECKEDC_BRANCH=master
) else if "%CHECKEDC_BRANCH%"=="" (
  set CHECKEDC_BRANCH=master
)

if not defined CLANG_BRANCH (
  if defined BUILD_SOURCEBRANCHNAME (
    set CLANG_BRANCH=%BUILD_SOURCEBRANCHNAME%
  ) else (
    set CLANG_BRANCH=master
  )
) else if "%CLANG_BRANCH%"=="" (
    set CLANG_BRANCH=master
)

if not defined SIGN_BRANCH (
  set SIGN_BRANCH=master
) else if "%SIGN_BRANCH%"=="" (
  set SIGN_BRANCH=master
)

rem set up source versions (Git commit number)
if not defined CHECKEDC_COMMIT (
  set CHECKEDC_COMMIT=HEAD
)

if not defined CLANG_COMMIT (
  set CLANG_COMMIT=HEAD
)

if NOT DEFINED MSBUILD_BIN (
 @rem used to be: set "MSBUILD_BIN=%programfiles(x86)%\MSBuild\15.0\Bin\MSBuild.exe"
 set "MSBUILD_BIN=%programfiles(x86)%\Microsoft Visual Studio\2017\Enterprise\MSBuild\15.0\Bin\MSBuild.exe"
)

if NOT DEFINED MSBUILD_CPU_COUNT (
  if DEFINED NUMBER_OF_PROCESSORS (
    set MSBUILD_CPU_COUNT=%NUMBER_OF_PROCESSORS%
  ) else (
    set MSBUILD_CPU_COUNT=2
  )
)

if NOT DEFINED CL_CPU_COUNT (
  rem It would be better to calculate this based on total physical memory availablity.
  set CL_CPU_COUNT=4
)

echo Configured environment variables:
echo.
echo.  BUILDCONFIGURATION: %BUILDCONFIGURATION%
echo.  BUILDOS: %BUILDOS%
echo.  TEST_TARGET_ARCH: %TEST_TARGET_ARCH%
echo.  TEST_SUITE: %TEST_SUITE%
echo.  SKIP_CHECKEDC_TESTS: %SKIP_CHECKEDC_TESTS%
echo.  BUILD_CHECKEDC_CLEAN: %BUILD_CHECKEDC_CLEAN%
echo.  BUILD_PACKAGE: %BUILD_PACKAGE%
echo.  SIGN_INSTALLER: %SIGN_INSTALLER%
echo.
echo.  Directories:
echo.    BUILD_SOURCESDIRECTORY: %BUILD_SOURCESDIRECTORY%
echo.    BUILD_BINARIESDIRECTORY: %BUILD_BINARIESDIRECTORY%
echo.    LLVM_OBJ_DIR: %LLVM_OBJ_DIR%
echo.
echo.  Branch and commit information:
echo.    CLANG_BRANCH: %CLANG_BRANCH%
echo.    CLANG_COMMIT: %CLANG_COMMIT%
echo.    CHECKEDC BRANCH: %CHECKEDC_BRANCH%
echo.    CHECKEDC_COMMIT: %CHECKEDC_COMMIT%
echo.    SIGN_BRANCH: %SIGN_BRANCH%
echo.
echo.  MSBUILD_BIN: %MSBUILD_BIN%
echo.  MSBUILD_CPU_COUNT: %MSBUILD_CPU_COUNT%
echo.  CL_CPU_COUNT: %CL_CPU_COUNT%

exit /b 0
