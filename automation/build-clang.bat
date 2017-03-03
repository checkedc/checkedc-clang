rem Build clang in Visual Studio Team Services
rem
rem The MSBuild task in Visual Studio uses a relative path to the
rem solution file, which does not work for CMake.   So create a
rem a script and just invoke MSBuild directly.

set MSBUILDBIN=%programfiles(x86)%\MSBuild\14.0\Bin\MSBuild.exe
cd %LLVM_OBJ_DIR%
"%MSBUILD_BIN%" tools\clang\tools\driver\clang.vcxproj /maxcpucount:%MSBUILD_CPU_COUNT%


