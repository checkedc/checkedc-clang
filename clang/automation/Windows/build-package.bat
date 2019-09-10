rem Build an installation package for clang.
rem
rem The MSBuild task in Visual Studio uses a relative path to the
rem solution file, which does not work for CMake-generated files, 
rem which should be generated outside the source tree   So create a
rem a script and just invoke MSBuild directly.

set OLD_DIR=%CD%
cd %LLVM_OBJ_DIR%

if "%BUILD_PACKAGE%" == "No" (goto succeeded)

echo.Building installation package for clang
"%MSBUILD_BIN%" PACKAGE.vcxproj /p:Configuration=%BUILDCONFIGURATION% /v:%MSBUILD_VERBOSITY% /maxcpucount:%MSBUILD_CPU_COUNT% /p:CL_MPCount=%CL_CPU_COUNT%
 if ERRORLEVEL 1 (goto cmdfailed)

rem Put the installer executable in its own subdirectory.  The VSTS build
rem artifact copy task can only copy directories or specifically named files
rem (no wild cards), and the installer executable name includes a version
rem number.

 move LLVM-*.exe package
  if ERRORLEVEL 1 (goto cmdfailed)

:succeeded
  cd %OLD_DIR%
  exit /b 0

:cmdfailed
  echo.Build installation package failed.
  cd %OLD_DIR%
  exit /b 1


