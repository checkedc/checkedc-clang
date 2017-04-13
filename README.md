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

### Summary
We are implementing a subset of the Checked C extension that can be used to add bounds
checking to real-world C programs.  After that, we will expand the implementation to include
additional Checked C features. The subset includes the new `ptr`, `array_ptr`, and `checked` 
array types. It also includes in-line bounds declarations, bounds-safe
interface annotations, the new cast operators, and checked blocks.
The implementation of the subset will be end-to-end within the
compiler: it will include parsing, typechecking, other static
semantic analysis, and code generation.

We have completed most of the parsing and typechecking work
for the subset. We are working on the insertion of runtime bounds checks.
We have yet to start on implementing checked blocks, the new cast operators, 
and checking the correctness of bounds declarations at compile time.

### Details

This table summarizes the implementation status for the
features of the subset.  The columns are the major phases of the compiler
and the rows list the language features.    A '-' indicates that that a compiler
phase is not applicable to the language feature.

|Feature                     | Parsing     | Type checking | Other semantic analysis| Code generation |
|----                        | ---         | ---           | ---                    | ----            |
|`ptr` type                  | Done        | Done          | -                      | Done             |
|`array_ptr` type           | Done        | Done          | -                      | Done (excluding checks) |
|`checked` array type       | Done        | Done          | -                      | Done (excluding checks) |
|In-line bounds declarations | Done        | Done          | In-progress            | -            |
|Bounds-safe interfaces      | Done        | Done          | Done                   | -            |
|Function types with bounds-safe interfaces|Done | Done    | -                      | -             |
|Checking of redeclarations  | -           | -             | Done                   |              | 
|Expression bounds inference | -           | -             | In-progress            | -             |
|Insertion of bounds checks  | -           | -             | -                      | In-progress   |
|Insertion of null checks    | -           | -             | -                      | Not started   |
|Checking correctness of bounds declarations | -   | -     | Not started            | -             |
|Relative alignment of bounds declarations | Done  | Done  | Not started            | -             |
|Checked blocks              | In progress | -             | Not started            | -             |
|New cast operators          | Not started | Not started   | Not started            | -             |

This table describes features _not_ in the subset, in approximate order of priority of implementation.

|Feature                  | Comments                             |
|-----                    |-----                                 |
|Null-terminated arrays   |                                      |
|Restrict taking addresses of variables used in bounds       |   |
|Restrict taking addresses of members used in member bounds  |   |
|Flow-sensitive bounds declarations                        |   |
|Where clauses                                             |   |
|Checking correctness of where clauses                     |   |
|Bundled blocks                                            |   |
|Holds/suspend state of member bounds                      | Depends on flow-sensitive bounds declarations. |
|Check for undefined order of evaluation issues            |   |
|Overflow checking of `array_ptr` pointer arithmetic      |   |
|Span types                                                |Lower priority|
|Pointers directly to `array_ptr`s                       |Design is tentative.|

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
