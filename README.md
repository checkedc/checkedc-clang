# The Checked C clang repo

This repo contains a version of clang that is being modified to support Checked C.  Checked C 
extends C with checking to detect or prevent common programming  errors such as
out-of-bounds memory accesses.  The Checked C specification is available  at the 
[Checked C repo](https://github.com/Microsoft/checkedc).

## Build Status

|Configuration|Testing|Status|
|--------|---------------|-------|
|Debug X86 Windows| Checked C and clang regression tests|![Debug X86 Windows status](https://msresearch.visualstudio.com/_apis/public/build/definitions/f6454e27-a46c-49d9-8453-29d89d53d2f9/211/badge)|
|Debug X64 Windows| Checked C and clang regression tests| ![Debug X64 Windows status](https://msresearch.visualstudio.com/_apis/public/build/definitions/f6454e27-a46c-49d9-8453-29d89d53d2f9/205/badge)|
|Debug X64 Linux  | Checked C and clang regression tests| ![Debug X64 Linux status](https://msresearch.visualstudio.com/_apis/public/build/definitions/f6454e27-a46c-49d9-8453-29d89d53d2f9/217/badge)|
|Release X64 Linux| Checked C, clang, and LLVM nightly tests|![Release X64 Linux status](https://msresearch.visualstudio.com/_apis/public/build/definitions/f6454e27-a46c-49d9-8453-29d89d53d2f9/238/badge)|

## News

We updated the LLVM/clang sources that we are using on June 20th, 2017.  If you have built your own copy
of the compiler, you will need to delete your cmake build directory and re-run cmake after you sync.

## Trying out Checked C

Programmers are welcome to ``kick the tires'' on Checked C as it is being implemented.
You will have to build your own copy of the compiler for now.  The compiler code and tests
are in multiple repos.

- [Setup and Build](docs/checkedc/Setup-and-Build.md) describes the organization of the code
and tests, how to set up a development machine to build clang, and how to build clang.
- [Testing](docs/checkedc/Testing.md) describes how to test the compiler once you have built it.
- The [Implementation Notes](docs/checkedc/Implementation-Notes.md) describe the implementation of Checked C
   in LLVM\clang.

After you have built the compiler, simply add the `-fcheckedc-extension` flag to your
command-line to enable the Checked C extension.

## Compiler development status

We are implementing a subset of the Checked C extension that can be used to add bounds
checking to real-world C programs.   The implementation roadmap and status are on the
[Checked C Wiki](https://github.com/Microsoft/checkedc-clang/wiki/Implementation-roadmap-and-status).

## Contributing

We welcome contributions to the Checked C project.  To get involved in the project, see
[Contributing to Checked C](https://github.com/Microsoft/checkedc/blob/master/CONTRIBUTING.md).   We have
a wish list of possible projects there.   

For code contributions, we follow the standard
[Github workflow](https://guides.github.com/introduction/flow/).  See 
[Contributing to Checked C](https://github.com/Microsoft/checkedc/blob/master/CONTRIBUTING.md) for more detail.
You will need to sign a contributor license agreement before contributing code.

## Code of conduct

This project has adopted the
[Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the
[Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any
additional questions or comments.
