// Test that non-canWrite files are constrained not to change so that the final
// annotations of other files are consistent with the original annotations of
// the non-canWrite files. The currently supported cases are function and
// variable declarations and checked regions.
// (https://github.com/correctcomputation/checkedc-clang/issues/387)

// TODO: When https://github.com/correctcomputation/checkedc-clang/issues/327 is
// fixed, replace the absolute -I option with a .. in the #include directive.
//
// TODO: Windows compatibility?

// "Lower" case: -base-dir should default to the working directory, so we should
// not allow canwrite_constraints_function_and_variable.h to change, and the
// internal types of q and the return should remain wild.
//
// RUN: cd %S && 3c -addcr -extra-arg=-I${PWD%/*} -output-postfix=checked -warn-all-root-cause -verify %s
// RUN: FileCheck -match-full-lines -check-prefixes=CHECK_LOWER --input-file %S/canwrite_constraints_function_and_variable.checked.c %s
// RUN: test ! -f %S/../canwrite_constraints_function_and_variable.checked.h
// RUN: rm %S/canwrite_constraints_function_and_variable.checked.c

// "Higher" case: When -base-dir is set to the parent directory, we can change
// canwrite_constraints_function_and_variable.h, so both q and the return should
// become checked.
//
// RUN: cd %S && 3c -addcr -extra-arg=-I${PWD%/*} -base-dir=${PWD%/*} -output-postfix=checked %s
// RUN: FileCheck -match-full-lines -check-prefixes=CHECK_HIGHER --input-file %S/canwrite_constraints_function_and_variable.checked.c %s
// RUN: FileCheck -match-full-lines -check-prefixes=CHECK_HIGHER --input-file %S/../canwrite_constraints_function_and_variable.checked.h %S/../canwrite_constraints_function_and_variable.h
// RUN: rm %S/canwrite_constraints_function_and_variable.checked.c %S/../canwrite_constraints_function_and_variable.checked.h

#include "canwrite_constraints_function_and_variable.h"

int *bar(int *q) {
  // CHECK_LOWER: int *bar(int *q : itype(_Ptr<int>)) : itype(_Ptr<int>) {
  // CHECK_HIGHER: _Ptr<int> bar(_Ptr<int> q) {
  foo(q);
  return foo_var;
}
