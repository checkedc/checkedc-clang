rem Create directories and sync files

if "%TEST_TARGET_ARCH%"=="AMD64" (
  set CMAKE_GENERATOR=-G "Visual Studio 15 2017 Win64"
) else (
  rem There is intentionally a blank space after the equal here, to force this to be
  rem an empty string.
  set CMAKE_GENERATOR= 
)

if "%BUILD_PACKAGE%"=="Yes" (
  if "%BUILDCONFIGURATION%"=="Release" (
   set EXTRA_FLAGS="-DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_INSTALL_TOOLCHAIN_ONLY=ON -DLLVM_USE_CRT_RELEASE=MT"
  ) else (
    set EXTRA_FLAGS= 
  )
) else (
	set EXTRA_FLAGS=
)

set OLD_DIR=%CD%

cd %LLVM_OBJ_DIR%
rem  This generates a build system that supports multiple configurations.  Don't try setting CMAKE_BUILD_TYPE here 
rem because it is ignored.
cmake %CMAKE_GENERATOR% -T "host=x64" %EXTRA_FLAGS% -DLLVM_INSTALL_TOOLCHAIN_ONLY=ON %BUILD_SOURCESDIRECTORY%\llvm
if ERRORLEVEL 1 (goto cmdfailed)

:succeeded
  cd %OLD_DIR%
  exit /b 0

:cmdfailed
  echo.Running CMake failed
  cd %OLD_DIR%
  exit /b 1



