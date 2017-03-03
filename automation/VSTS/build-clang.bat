rem Build clang in Visual Studio Team Services
rem
rem The MSBuild task in Visual Studio uses a relative path to the
rem solution file, which does not work for CMake.   So create a
rem a script and just invoke MSBuild directly.

set OLD_DIR=%CD%

cd %LLVM_OBJ_DIR%
"%MSBUILD_BIN%" tools\clang\tools\driver\clang.vcxproj /v:%MSBUILD_VERBOSITY% /maxcpucount:%MSBUILD_CPU_COUNT%
if ERRORLEVEL 1 (goto cmdfailed)

:succeeded
  cd %OLD_DIR%
  exit /b 0

:cmdfailed
  echo.Building clang failed
  cd %OLD_DIR%
  exit /b 1



