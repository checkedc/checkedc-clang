# Summer Internship Project - 2021

## Introduction

This document gives a high level description of the project that the intern will be working on during this internship.

## Project Title 

Integrating Pre/Post Conditions and Invariants into Checked C.

## Project Goal 
The Checked C language allows programmers to specify preconditions on function parameters, postconditions on function return values, and program invariants using a `_Where` clause. The goal of this project is to integrate preconditions, postconditions and program invariants, specified using `_Where` clauses, into the Checked C compiler and use them to validate that:

1. function arguments at a call-site satisfy the preconditions on the corresponding function parameters,
2. the value returned by a function satisfies the postcondition on the return value of the function, and
3. the program invariants hold at the program points at which they are specified.

This validation entails building a framework to discharge proofs. 

An additional goal is to use the proof framework as an opt-in complement to strengthen the bounds validation of checked pointers, if the built-in bounds validation is unable to discharge a proof.  

## Project Motivation
Integrating pre/post conditions and program invariants, and building an alternate proof framework helps Checked C make further progress on the promise of type-safety. Additionally, we hope that the alternate proof framework will help reduce the number of compiler warnings and dynamic bounds casts.

## Project Approach
To achieve the goals of the project, we take the approach of building a proof framework that uses Z3, an SMT-based theorem prover to discharge proofs. At each program point and for each proof to be discharged, all the facts that are relevant to the proof like  1) the inferred bounds of a pointer, 2) the declared/widened bounds of the pointer, 3) the equivalent expressions available at the program point, and 4) the pre/post conditions and the program invariants that hold at the said program point, are converted into constraints in the theory of Bit Vectors that is supported by Z3.  The negation of the conjunction of these constraints is given to Z3 as input, for which Z3 tries to find a satisfying assignment. If Z3 is unable to find a satisfying assignment, then we say that Z3 has discharged the proof. 

## Examples
This section contains two examples that are motivated by code in the MUSL library. They illustrate the need to insert a `_Dynamic_bounds_cast` in order to avoid a compiler error/warning. With an Z3-based proof framework to support bounds validation, the Checked C compiler should be able to validate the bounds without the needing the programmer to insert `_Dynamic_bounds_casts`. But the Z3-based proof framework needs the user to annotate these programs with `_Where` clauses to specify program invariants. In the future, the hope is to infer many of these invariants automatically.

### Example 1:

```
int inetp(int af, _Array_ptr<unsigned char> a0 : byte_count(af == AF_INET ? 4 : 16))
{
    if (af == AF_INET) {
      _Array_ptr<unsigned char> a : bounds(a0, a0 + 4) = a0; // COMPILER ERR - UNABLE TO PROVE
      return 0;
    } else if (af != AF_INET6)
      return -1;

    _Array_ptr<unsigned char> a : bounds(a0, a0 + 16) = a0; // COMPILER ERR - UNABLE TO PROVE
    return 0;
}
```
To enable the compiler to discharge the proof in its current form, we need to modify this program as follows:
```
int inetp(int af, _Array_ptr<unsigned char> a0 : byte_count(af == AF_INET ? 4 : 16))
{
    if (af == AF_INET) {
      _Array_ptr<unsigned char> a : bounds(a0, a0 + 4) = 0;
      a = _Dynamic_bounds_cast<_Array_ptr<unsigned char>>(a0, count(4));
      return 0;
    } else if (af != AF_INET6)
      return -1;

    _Array_ptr<unsigned char> a : bounds(a0, a0 + 16) = 0;
    a = _Dynamic_bounds_cast<_Array_ptr<unsigned char>>(a0, count(16));
    return 0;
}
```
Alternatively, we could add `_Where` clause annotations that will suffice for the SMT-based bounds validation to succeed:
```
int inetp(int af, _Array_ptr<unsigned char> a0 : byte_count(af == AF_INET ? 4 : 16))
{
    if (af == AF_INET) {
      _Where af == AF_INET;
      _Array_ptr<unsigned char> a : bounds(a0, a0 + 4) = a0; // Statement A
      return 0;
    } else if (af != AF_INET6)
      return -1;

    _Where af == AF_INET6;
    _Array_ptr<unsigned char> a : bounds(a0, a0 + 16) = a0;
    return 0;
}
```
Constraints that will be input to Z3 to validate the bounds (shown only for statement A) : 

Note that in the constraints below, `AF_INET` is a numerical constant.

```
NOT(
   a >= a0 AND a < a0 + 4                               // from the declared bounds of a
   AND (
          (a >= a0 AND a < a0 + 4 AND af == AF_INET)
          OR
          (a >= a0 AND a < a0 + 16 AND af != AF_INET)
       )                                                // from the inferred bounds of a
   AND af == AF_INET                                    // from the program invariant
   AND a >= 0                                           // from the type of a
   AND a0 >= 0                                          // from the type of a0
   )
```


### Example 2:
```
void foo (_Array_ptr<int> p : count(n), unsigned int n);

void bar(_Array_ptr<int> a : count (m), unsigned int m, unsigned int n) {
   if (n + 3 > m || n < 1)
     return;
   foo(a + m - n, n); // COMPILER WARNING - UNABLE TO PROVE
}
```
To enable the compiler to discharge the proof in its current form, we need to modify this program as follows:
```
void foo (_Array_ptr<int> p : count(n), unsigned int n);

void bar(_Array_ptr<int> a : count (m), unsigned int m, unsigned int n) {
   if (n + 3 > m || n < 1)
     return;
   _Array_ptr<int> arg_a : count(n) = _Dynamic_bounds_cast<_Array_ptr<int>>(a + m - n, count(n));
   foo(arg_a, n);
}
```
Alternatively, we could add `_Where` clause annotations that will suffice for the SMT-based bounds validation to succeed:
```
void foo (_Array_ptr<int> p : count(n), unsigned int n);

void bar(_Array_ptr<int> a : count(m), unsigned int m, unsigned int n) {
   if (n + 3 > m || n < 1)
     return;
   _Where n + 3 <= m _And n >= 1;
   foo(a + m - n, n);
}
```
Constraints that will be input to Z3 to validate the bounds of the first argument to function `foo` (we will give it the name `arg_a`):
```
NOT(
   arg_a >= a AND a  arg_a < a + m          // from the inferred bounds of arg_a
   AND arg_a == a + m - n                   // from implicit assignment
   AND arg_a >= arg_a AND arg_a < arg_a + n // from expected bounds of arg_a
   AND n + 3 <= m                           // from the program invariant
   AND n >= 1                               // from the program invariant
   AND n >= 0                               // from the type of n (unsigned int)
   AND m >= 0                               // from the type of m (unsigned int)
   AND arg_a >= 0                           // from the type of arg_a (_Array_ptr<int>)
   AND a >= 0                               // from the type of a (_Array_ptr<int>)
   )
```

## High-level Plan for Project Execution
The main chunks of work are:
 - Computing the set of pre/post conditions and program invariants that are available at a given program point - for this we will modify the existing `AvailableFacts` dataflow analysis.
 - Interfacing Z3 to the Checked C compiler:
    - APIs to extract constraints from bounds expressions, types of the variables involved, available expressions and the available (`_Where` clause) facts.
    - Invocation of Z3 on these constraints, communicating the result back to the compiler, and handling diagnostics.
- Testing, evaluation and collection of metrics:
    - Writing a set of test cases that serve as micro benchmarks.
    - Writing scripts and adding profiling information in the compiler to collect relevant metrics (number of successful proofs by Z3, compile-time overhead, number of constraints/variables by adding profiling counters inside the compiler, and number of where clauses added, number of dynamic bounds casts eliminated by executing scripts on the test cases and benchmarks).
    - Evaluating this project on benchmarks and practical code.

## Code Details
 - The branch `internship-2021-checkins` in the `checkedc-clang` repository contains:
    - The code for dumping Where clauses during AST dumps, and
    - The code for the new AvailableFacts analysis as per the design document Available-Facts-Analysis-New.md

 - The branch `internship-2021-checkins-smt-prover` in the `checkedc-clang` repository contains:
    - The prototype implementation of the SMT-based proof framework using Z3.
