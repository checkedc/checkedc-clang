rem Create directories and sync files

if not exist %BUILD_SOURCESDIRECTORY%\llvm (
  git clone -c core.autocrlf=false https://github.com/Microsoft/checkedc-llvm %BUILD_SOURCESDIRECTORY%\llvm
  git clone -c core.autocrlf=false https://github.com/Microsoft/checkedc-clang %BUILD_SOURCESDIRECTORY%\llvm\tools\clang
  git clone https://github.com/Microsoft/checkedc %BUILD_SOURCESDIRECTORY%\llvm\projects\checkedc-wrapper\checkedc
)

cd %BUILD_SOURCESDIRECTORY%\llvm
git pull origin master
cd %BUILD_SOURCESDIRECTORY%\llvm\projects\checkedc-wrapper\checkedc
git pull origin master
cd %BUILD_SOURCESDIRECTORY%\llvm\tools\clang
git pull origin master

if not exist %BUILD_BINARIESDIRECTORY%\llvm.obj (
  mkdir %BUILD_BINARIESDIRECTORY%\llvm.obj
)

