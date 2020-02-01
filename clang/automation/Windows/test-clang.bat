rem Test clang in Visual Studio Team Services
rem
rem The MSBuild task in Visual Studio uses a relative path to the
rem solution file, which does not work for CMake-generated files, 
rem which should be genereated outside the source tree   So create a
rem a script and just invoke MSBuild directly.

set OLD_DIR=%CD%

cd %LLVM_OBJ_DIR%

if "%SKIP_CHECKEDC_TESTS%"=="Yes" (
  rem
) else (
  "%MSBUILD_BIN%" projects\checkedc-wrapper\check-checkedc.vcxproj /p:Configuration=%BUILDCONFIGURATION% /v:%MSBUILD_VERBOSITY% /maxcpucount:%MSBUILD_CPU_COUNT% /p:CL_MPCount=%CL_CPU_COUNT%
  if ERRORLEVEL 1 (goto cmdfailed)
)

if "%TEST_SUITE%"=="CheckedC" (
  rem
) else if "%TEST_SUITE%"=="CheckedC_clang" (
  "%MSBUILD_BIN%" tools\clang\test\check-clang.vcxproj /p:Configuration=%BUILDCONFIGURATION% /v:%MSBUILD_VERBOSITY% /maxcpucount:%MSBUILD_CPU_COUNT% /p:CL_MPCount=%CL_CPU_COUNT%
  if ERRORLEVEL 1 (goto cmdfailed)
) else if "%TEST_SUITE%"=="CheckedC_LLVM" (
  "%MSBUILD_BIN%" check-all.vcxproj /p:Configuration=%BUILDCONFIGURATION% /v:%MSBUILD_VERBOSITY% /maxcpucount:%MSBUILD_CPU_COUNT% /p:CL_MPCount=%CL_CPU_COUNT%
  if ERRORLEVEL 1 (goto cmdfailed)
)

:succeeded
  cd %OLD_DIR%
  exit /b 0

:cmdfailed
  echo.Clang tests failed
  cd %OLD_DIR%
  exit /b 1
confi


