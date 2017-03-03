rem Test clang in Visual Studio Team Services
rem
rem The MSBuild task in Visual Studio uses a relative path to the
rem solution file, which does not work for CMake-generated files, 
rem which should be genereated outside the source tree   So create a
rem a script and just invoke MSBuild directly.


cd %LLVM_OBJ_DIR%

"%MSBUILD_BIN%" tools\clang\tools\driver\clang.vcxproj /maxcpucount:%MSBUILD_CPU_COUNT%

if "%TEST_SUITE%"=="CheckedC" (
  rem
) else if "%TEST_SUITE%"=="CheckedC_clang" (
  "%MSBUILD_BIN%" tools\clang\tools\driver\clang.vcxproj /maxcpucount:%MSBUILD_CPU_COUNT%
) else if "%TEST_SUITE%"=="CheckedC_LLVM" (
  "%MSBUILD_BIN%" check-all.vcxproj /maxcpucount:%MSBUILD_CPU_COUNT%
)





