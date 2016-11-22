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

## Status

### Summary
We are close to having a complete implementation of the new `_Ptr` types and the
language features for using them.   We also have support for parsing and typechecking bounds
expressions and bounds declarations.  We are working on interoperation support now .   To able
to use the Checked C extension in existing code bases, we need the interoperation support,
so we are implementing it before other features.

### Details
We have:

- Extended LLVM/clang with a feature flag `-fcheckedc-extension`.  This flag is valid only for C programs.
   It cannot be used with C++, Objective C, or OpenCL.
- Implemented parsing and typechecking for the new `_Ptr`, `_Array_ptr`, and `_Checked` array types,
   including implicit conversions described in the Checked C specification.   The `_Array_ptr` and
  `_Checked` array types do not have any runtime bounds checking yet.
- Implemented parsing and typechecking for bounds expression and in-line bounds declarations.
- Implemented parsing and typechecking for bounds-safe interfaces, including the implicit conversions
  done at bounds-safe interfaces.

We are now:

- Implementing function types with bounds information.
- Implementing type checking of redeclarations of variables and functions with bounds
information.  When this is finished, interoperation support will be mostly done.
Programmers will be able to redeclare existing variables and functions with additional bounds information.

After that, we will begin implementing static semantics checking for programs that use `_Ptr`
pointers and `_Array_ptr` pointers to constant-sized data.  This includes

- Checking the correctness of bounds declarations for constant-sized data.
- Checking that casts to `_Ptr` types from `_Array_ptr` types are bounds-safe.

## Compiler development

Programmers are welcome to ``kick the tires'' on Checked C as it is being implemented.
You will have to build your own copy of the compiler for now:

- [Setup and Build](docs/checkedc/Setup-and-Build.md) describes the organization of the code,
how to set up a development machine to build clang, and how to build clang.
- [Testing](docs/checked/Testing.md) describes how to test the compiler once you have built it.
- The [Implementation Notes](docs/checkedc/Implementation-Notes.md) describe the implementation of Checked C
   in LLVM\clang.

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
