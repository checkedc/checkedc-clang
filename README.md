# The Checked C clang repo

This repo contains a version of clang that is being modified to support Checked C.  Checked C is
an extension to C that adds checking to detect or prevent common programming  errors such as
out-of-bounds memory accesses.  The Checked C specification is available  at the 
[Checked C repo](https://github.com/Microsoft/checkedc).


The code for the Checked C version of LLVM/clang lives in two repos: the 
[Checked C clang repo](https://github.com/Microsoft/checked-clang)
and the [Checked C LLVM repo](https://github.com/Microsoft/checkedc-llvm).  Each repo is licensed 
under the [University of Illinois/NCSA license](https://opensource.org/licenses/NCSA).
The tests for Checked C live in the [Checked C repo](https://github.com/Microsoft/checkedc).  These are
language conformance tests, so they are placed with the specification, not the compiler.
The test code is licensed under the [MIT license](https://opensource.org/licenses/MIT).
See the file LICENSE.TXT in each repo for complete details of licensing.  

## Trying out Checked C

Programmers are welcome to ``kick the tires'' on Checked C as it is being implemented.
You will have to build your own copy of the compiler for now (we are working on getting
a nightly compiler build going):

- [Setup and Build](docs/checkedc/Setup-and-Build.md) describes the organization of the code,
how to set up a development machine to build clang, and how to build clang.
- [Testing](docs/checkedc/Testing.md) describes how to test the compiler once you have built it.
- The [Implementation Notes](docs/checkedc/Implementation-Notes.md) describe the implementation of Checked C
   in LLVM\clang.

After you have built the compiler, simply add the `-fcheckedc-extension` flag to your
command-line to enable the Checked C extensions.

## Compiler development status

### Summary
We are implementing a subset of the Checked C extension that can be used to add bounds 
checking to real-world C programs.  After that, we will expand the implementation to include
additional Checked C features. The subset includes the new `_Ptr`, `_Array_ptr`, and `checked` 
array types. It also includes in-line bounds declarations, bounds-safe
interface annotations, the new cast operators, and checked blocks.
The implementation of the subset will be end-to-end with the
compiler: it will include parsing, typechecking, other static checks,
and code generation.

At this point, we have completed most of the parsing and typechecking work
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
|`checked` array type        | Done        | Done          | -                      | Done (excluding checks) |
|In-line bounds declarations | Done        | Done          | In-progress            | -            |
|Bounds-safe interfaces      | Done        | Done          | Done                   | -            |
|Function types with bounds-safe interfaces|Done | Done    | -                      | -             |
|Checking of redeclarations  | -           | -             | Done                   |              | 
|Expression bounds inference | -           | -             | In-progress            | -             |
|Insertion of bounds checks  | -           | -             | -                      | In-progress   |
|Insertion of null checks    | -           | -             | -                      | Not started   |
|Checking correctness of bounds declarations | -   | -     | Not started            | -             |
|Relative alignment of bounds declarations | Not started| _| Not started            | -             |
|Checked blocks              | Not started | -             | Not started            | -             |
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
