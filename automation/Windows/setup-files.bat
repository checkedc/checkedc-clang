rem Create directories and sync files

set OLD_DIR=%CD%

if "%BUILD_CHECKEDC_CLEAN%"=="Yes" (
  if exist %BUILD_SOURCESDIRECTORY%\llvm (
    rmdir /s /q %BUILD_SOURCESDIRECTORY%\llvm
    if ERRORLEVEL 1 (goto cmdfailed)
  )
  if exist %LLVM_OBJ_DIR% (
    rmdir /s /q %LLVM_OBJ_DIR%
    if ERRORLEVEL 1 (goto cmdfailed)
  )
)
 
if not exist %BUILD_SOURCESDIRECTORY%\llvm\.git (
  git clone -c core.autocrlf=false https://github.com/Microsoft/checkedc-llvm %BUILD_SOURCESDIRECTORY%\llvm
  if ERRORLEVEL 1 (goto cmdfailed)
)

if not exist %BUILD_SOURCESDIRECTORY%\llvm\tools\clang\.git (
  git clone -c core.autocrlf=false https://github.com/Microsoft/checkedc-clang %BUILD_SOURCESDIRECTORY%\llvm\tools\clang
  if ERRORLEVEL 1 (goto cmdfailed)
)

if not exist %BUILD_SOURCESDIRECTORY%\llvm\projects\checkedc-wrapper\checkedc\.git (
  git clone https://github.com/Microsoft/checkedc %BUILD_SOURCESDIRECTORY%\llvm\projects\checkedc-wrapper\checkedc
  if ERRORLEVEL 1 (goto cmdfailed)
)


if "%SIGN_INSTALLER%" NEQ "No" (
  if not exist %BUILD_SOURCESDIRECTORY%\automation\Windows\sign\.git (
    rem VSO automation runs scripts from a top-level clang repo that its cloned.
    rem Place the signing scripts there, not within the cloned compiler repos.
    git -c http.extraheader="Authorization: bearer %SYSTEM_ACCESSTOKEN%" clone https://msresearch.visualstudio.com/DefaultCollection/CheckedC/_git/checkedc-sign %BUILD_SOURCESDIRECTORY%\automation\Windows\sign
    if ERRORLEVEL 1 (goto cmdfailed)
  )
)

rem set up LLVM sources
cd %BUILD_SOURCESDIRECTORY%\llvm
if ERRORLEVEL 1 (goto cmdfailed)
git fetch origin
if ERRORLEVEL 1 (goto cmdfailed)
git checkout -f %LLVM_BRANCH%
if ERRORLEVEL 1 (goto cmdfailed)
git pull -f origin %LLVM_BRANCH%
if ERRORLEVEL 1 (goto cmdfailed)

git checkout %LLVM_COMMIT%
if ERRORLEVEL 1 (goto cmdfailed)

rem set up Checked C sources
cd %BUILD_SOURCESDIRECTORY%\llvm\projects\checkedc-wrapper\checkedc
if ERRORLEVEL 1 (goto cmdfailed)
git fetch origin
if ERRORLEVEL 1 (goto cmdfailed)
git checkout -f %CHECKEDC_BRANCH%
if ERRORLEVEL 1 (goto cmdfailed)
git pull -f origin %CHECKEDC_BRANCH%
if ERRORLEVEL 1 (goto cmdfailed)

git checkout %CHECKEDC_COMMIT%
if ERRORLEVEL 1 (goto cmdfailed)

rem Set up clang sources
cd %BUILD_SOURCESDIRECTORY%\llvm\tools\clang
if ERRORLEVEL 1 (goto cmdfailed)
git fetch origin
if ERRORLEVEL 1 (goto cmdfailed)
git checkout -f %CLANG_BRANCH%
if ERRORLEVEL 1 (goto cmdfailed)
git pull -f origin %CLANG_BRANCH%
if ERRORLEVEL 1 (goto cmdfailed)

git checkout %CLANG_COMMIT%
if ERRORLEVEL 1 (goto cmdfailed)

if not exist %LLVM_OBJ_DIR% (
  mkdir %LLVM_OBJ_DIR%
  if ERRORLEVEL 1 (goto cmdfailed)
)

rem Set up sources for scripts for signing installer
if "%SIGN_INSTALLER%" NEQ "No" (
    cd %BUILD_SOURCESDIRECTORY%\automation\Windows\sign
    if ERRORLEVEL 1 (goto cmdfailed)
    git -c http.extraheader="Authorization: bearer %SYSTEM_ACCESSTOKEN%" fetch origin
    if ERRORLEVEL 1 (goto cmdfailed)
    git -c http.extraheader="Authorization: bearer %SYSTEM_ACCESSTOKEN%" checkout -f %SIGN_BRANCH%
    if ERRORLEVEL 1 (goto cmdfailed)
    git -c http.extraheader="Authorization: bearer %SYSTEM_ACCESSTOKEN%" pull -f origin %SIGN_BRANCH%
    if ERRORLEVEL 1 (goto cmdfailed)
)

rem Set up directory for package
if exist %LLVM_OBJ_DIR%\package (
  rmdir /s /q %LLVM_OBJ_DIR%\package
  if ERRORLEVEL 1 (goto cmdfailed)
)

if "%BUILD_PACKAGE%"=="Yes" (
  mkdir %LLVM_OBJ_DIR%\package
  if ERRORLEVEL 1 (goto cmdfailed)
)

rem Set up directory for signing
if exist %LLVM_OBJ_DIR%\signed-package (
  rmdir /s /q %LLVM_OBJ_DIR%\signed-package
  if ERRORLEVEL 1 (goto cmdfailed)
)

if "%SIGN_INSTALLER%" NEQ "No" (
  mkdir %LLVM_OBJ_DIR%\signed-package
  if ERRORLEVEL 1 (goto cmdfailed)
)

:succeeded
  cd %OLD_DIR%
  exit /b 0

:cmdfailed
  echo.Setting up files failed
  cd %OLD_DIR%
  exit /b 1
