rem Create directories and sync files

if "%TEST_TARGET_ARCH%"=="AMD64" (
  set CMAKE_GENERATOR=-G "Visual Studio 15 2017 Win64"
) else (
  rem There is intentionally a blank space after the equal here, to force this to be
  rem an empty string.
  set CMAKE_GENERATOR= 
)

set OLD_DIR=%CD%

cd %LLVM_OBJ_DIR%
cmake %CMAKE_GENERATOR% -T "host=x64" -DCMAKE_BUILD_TYPE=%BUILDCONFIGURATION% %BUILD_SOURCESDIRECTORY%\llvm
if ERRORLEVEL 1 (goto cmdfailed)

:succeeded
  cd %OLD_DIR%
  exit /b 0

:cmdfailed
  echo.Running CMake failed
  cd %OLD_DIR%
  exit /b 1



