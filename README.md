# The Checked C clang repo

This repo contains a version of clang that is being modified to support Checked C.  Checked C is
an extension to C that adds checking to detect or prevent common programming  errors such as
out-of-bounds memory accesses.  The Checked C specification is available  at the 
[Checked C repo](https://github.com/Microsoft/checkedc).

## Code organization

The code for the Checked C version of LLVM/clang lives in two repos: this repo and the
[Checked LLVM repo](https://github.com/Microsoft/checkedc-llvm).  Each repo is licensed 
under the [University of Illinois/NCSA license](https://opensource.org/licenses/NCSA).
See the file LICENSE.txt for complete details of licensing.  
LLVM uses subversion for distributed  source code control.   It is mirrored by Git repositories on Github: 
https://github.com/llvm-mirror/llvm and https://github.com/llvm-mirror/clang/.  The Checked C repos
pull from the Github repos.

The tests live in the [Checked C repo](https://github.com/Microsoft/checkedc).These are
language conformance tests, so it seems appropriate to place them with the specificaiton, not
with the compiler.  The test code is licensed under the [MIT license](https://opensource.org/licenses/MIT).

There are two branches in each repo:
- master: the main development branch  for Checked C.   All changes committed here have been code reviewed and passed testing.
- baseline:: these are pristine copies of the Github mirrors.   Do not commit changes for Checked C to the baseline branches.

## Setting up source code 
To set up your source code on your local machine,

- Clone LLVM to your desired location on your machine:
```
git clone https://github.com/Microsoft/checkedc-llvm
git checkout master
```
- Change to the llvm/tools directory and clone the clang repo to your machine:
```
git clone https://github.com/Microsoft/checkedc-clang
git checkout master
```
- Change to  llvm/projects/checkedc-wrapper and clone the Checked C repo your machine:
```
git clone https://github.com/Microsoft/checkedc
git checkout master
```

## Building

## Running tests

## Make contributions



