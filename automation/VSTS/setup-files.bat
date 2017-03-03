rem Create directories and sync files

if not exist %BUILD_SOURCESDIRECTORY%\llvm (
  git clone -c core.autocrlf=false https://github.com/Microsoft/checkedc-llvm %BUILD_SOURCESDIRECTORY%\llvm
  git clone -c core.autocrlf=false https://github.com/Microsoft/checkedc-clang %BUILD_SOURCESDIRECTORY%\llvm\tools\clang
  git clone https://github.com/Microsoft/checkedc %BUILD_SOURCESDIRECTORY%\llvm\projects\checkedc-wrapper\checkedc
)

rem set up LLVM sources
cd %BUILD_SOURCESDIRECTORY%\llvm
git pull origin %LLVM_BRANCH%
git checkout %LLVM_COMMIT%

rem set up Checked C sources
cd %BUILD_SOURCESDIRECTORY%\llvm\projects\checkedc-wrapper\checkedc
git pull origin %CHECKEDC_BRANCH%
git checkout %CHECKEDC_COMMIT%

rem Set up clang sources
cd %BUILD_SOURCESDIRECTORY%\llvm\tools\clang
git pull origin %CLANG_BRANCH%
git checkout %CLANG_COMMIT%


if not exist %LLVM_OBJ_DIR% (
  mkdir %LLVM_OBJ_DIR%
)
