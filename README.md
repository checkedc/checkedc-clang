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
See the file LICENSE.txt in each repo for complete details of licensing.  

## Status

The compiler code is being shared early in the process of extending LLVM/clang to support the Checked C
extension.  We have

- Extended LLVM/clang with a feature flag `-fcheckedc-extension`.  This flag is valid only for C programs.
  It cannot be used with C++, Objective C, or OpenCL.
- Implemented parsing and typechecking for the new `ptr`, `array_ptr`, and `checked` array types, 
  including implicit conversions described in Section 5.1.4 of the Checked C specification.  The new
  types are converted to unchecked types during compilation, so they do not have any bounds checking
  yet.

We are now working on parsing and typechecking of the new bounds declarations.

## Compiler development

The compiler is not far enough along for programmers to "kick the tires" on Checked C.   We do not have a
installable version clang available yet.  If you are really interested, you can build your own copy of the compiler:

- [Setup and Build](docs\CheckedC\Setup-and-Build.md) describes the organization of the code,
how to set up a development machine to build clang, and how to build clang.
- The [Implementation Notes](docs\CheckedC\Implementation-Notes.md) describe the implementation of Checked C
   in LLVM\clang.

## Contributing

We welcome contributions to the Checked C project.  To get involved in the project, see
[Contributing to Checked C](https://github.com/Microsoft/checkedc/blob/master/CONTRIBUTING.md).   We have
a wish list of possible projects there.   

For code contributions, we follow the standard
[Github workflow](https://guides.github.com/introduction/flow/).  See 
[Contributing to Checked C](https://github.com/Microsoft/checkedc/blob/master/CONTRIBUTING.md) for more detail.
You will need to sign a contributor license agreement before contributing code.
