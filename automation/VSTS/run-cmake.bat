rem Create directories and sync files

if /I "TEST_TARGET_ARCH"=="AMD64"(
  set CMAKE_GENERATOR=-G Visual Studio 14 2015 Win64
) else
  rem There is intentionally a blank space after the equal here, to force this to be
  rem an empty string.
  set CMAKE_GENERATOR= 
)

cd %LLVM_OBJDIR%
cmake %CMAKE_GENERATOR% -DCMAKE_BUILD_TYPE=%BUILDCONFIGURATION% %BUILD_SOURCESDIRECTORY%\llvm

