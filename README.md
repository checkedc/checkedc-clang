# The Checked C clang repo

This repo contains a version of the LLVM/Clang toolchain that is being modified
to support Checked C.  Checked C extends C with checking to detect or prevent
common programming  errors such as out-of-bounds memory accesses.  The Checked
C specification is available  at the 
[Checked C repo](https://github.com/Microsoft/checkedc).

## Announcement

Earlier this year, the LLVM community
[transitioned](https://forums.swift.org/t/llvm-monorepo-transition/25689) to
"monorepo". To align with upstream, we are now planning to transition Checked C
to a monorepo.

Starting the first week of Oct 2019, the checkedc-clang project will transition to
monorepo. This would result in the following changes:

1. [checkedc-llvm](https://github.com/Microsoft/checkedc-llvm) and
[checkedc-clang](https://github.com/Microsoft/checkedc-clang) (as well as other
LLVM subprojects) would be tracked via a single git repo.

2. The [checkedc-llvm](https://github.com/Microsoft/checkedc-llvm) repo would
no longer be maintained. The
[checkedc-clang](https://github.com/Microsoft/checkedc-clang) repo would be the
new monorepo.

3. There would be no changes to the
[checkedc](https://github.com/Microsoft/checkedc) repo. It would continue to be
a separate git repo.

4. All future patches should be based off this new monorepo.

5. You can use
[this](https://github.com/microsoft/checkedc-clang/blob/monorepo/clang/automation/UNIX/cherry-pick-to-monorepo.sh)
script to cherry-pick your existing patches to the new monorepo.

6. Make sure to set the following CMake flag to enable clang in your builds:
  `-DLLVM_ENABLE_PROJECTS=clang`

## Compiler source code update

On January 25, 2019, we updated to LLVM/Clang sources from January 16, 2019.
If you are building the Checked C clang compiler from source code, we suggest
you do a clean build by deleting your cmake object directory.

## We are hiring.

We have a position available for a [Principal Software
Engineer](https://careers.microsoft.com/us/en/job/559081/Principal-Software-Engineer)
or a [Senior Software
Engineer](https://careers.microsoft.com/us/en/job/570339/Senior-Software-Engineer).
We are looking for someone wih compiler and programming language implementation
experience who is passionate about making software more secure and reliable.

## Trying out Checked C

Programmers are welcome to "kick the tires" on Checked C as it is being
implemented.  We have pre-built compiler installers for Windows available for
download on the [release
page](https://github.com/Microsoft/checkedc-clang/releases).  For other
platforms, you will have to build your own copy of the compiler.  For
directions on how to do this, see the [Checked C clang
wiki](https://github.com/Microsoft/checkedc-clang/wiki).   The compiler user
manual is
[here](https://github.com/Microsoft/checkedc-clang/wiki/Checked-C-clang-user-manual).
For more information on Checked C and pointers to example code, see our
[Wiki](https://github.com/Microsoft/checkedc/wiki).

## More information

For more information on the Checked C clang compiler, see the [Checked C clang
wiki](https://github.com/Microsoft/checkedc-clang/wiki).

## Build Status

|Configuration|Testing|Status|
|--------|---------------|-------|
|Debug X86 Windows| Checked C and clang regression tests|![Debug X86 Windows status](https://msresearch.visualstudio.com/_apis/public/build/definitions/f6454e27-a46c-49d9-8453-29d89d53d2f9/211/badge)|
|Debug X64 Windows| Checked C and clang regression tests| ![Debug X64 Windows status](https://msresearch.visualstudio.com/_apis/public/build/definitions/f6454e27-a46c-49d9-8453-29d89d53d2f9/205/badge)|
|Debug X64 Linux  | Checked C and clang regression tests| ![Debug X64 Linux status](https://msresearch.visualstudio.com/_apis/public/build/definitions/f6454e27-a46c-49d9-8453-29d89d53d2f9/217/badge)|
|Release X64 Linux| Checked C, clang, and LLVM nightly tests|![Release X64 Linux status](https://msresearch.visualstudio.com/_apis/public/build/definitions/f6454e27-a46c-49d9-8453-29d89d53d2f9/238/badge)|

## Contributing

We welcome contributions to the Checked C project.  To get involved in the
project, see [Contributing to Checked
C](https://github.com/Microsoft/checkedc/blob/master/CONTRIBUTING.md).   We
have a wish list of possible projects there.   

For code contributions, we follow the standard [Github
workflow](https://guides.github.com/introduction/flow/).  See [Contributing to
Checked C](https://github.com/Microsoft/checkedc/blob/master/CONTRIBUTING.md)
for more detail.  You will need to sign a contributor license agreement before
contributing code.

## Code of conduct

This project has adopted the [Microsoft Open Source Code of
Conduct](https://opensource.microsoft.com/codeofconduct/).  For more
information see the [Code of Conduct
FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact
[opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional
questions or comments.
