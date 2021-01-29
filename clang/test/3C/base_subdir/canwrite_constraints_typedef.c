// An example of a case (typedefs) that is not yet handled by the canWrite
// constraints code and causes 3C to generate a change to an unwritable file.
// Test that 3C generates an error diagnostic.
// (https://github.com/correctcomputation/checkedc-clang/issues/387)

// TODO: Ditto the TODO comments from
// canwrite_constraints_function_and_variable.c re the RUN commands.
// RUN: cd %S && 3c -addcr -extra-arg=-I${PWD%/*} -verify %s | FileCheck -match-full-lines %s

// expected-error@unwritable_typedef.h:1 {{3C internal error: 3C generated changes to this file even though it is not allowed to write to the file}}
// expected-note@unwritable_typedef.h:1 {{-dump-unwritable-changes}}

#include "unwritable_typedef.h"

foo_typedef p = ((void *)0);

// To make sure we are testing what we want to test, make sure bar is rewritten
// as if foo_typedef is unconstrained. If foo_typedef were constrained, we'd
// expect bar to be rewritten differently.
int *bar(void) {
  // CHECK: _Ptr<int> bar(void) _Checked {
  return p;
  // Make sure 3C isn't inserting a cast or something clever like that.
  // CHECK: return p;
}
