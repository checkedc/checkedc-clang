// Like the base_subdir/canwrite_constraints_typedef test except that writing
// unwritable_typedef.h is not restricted by the base dir, so we reach the
// stdout mode restriction.

// RUN: 3c -base-dir=%S -addcr -verify %s | FileCheck -match-full-lines %s

// expected-error@unwritable_typedef.h:1 {{3C generated changes to this file, which is under the base dir but is not the main file and thus cannot be written in stdout mode}}
// expected-note@unwritable_typedef.h:1 {{-dump-unwritable-changes}}

#include "unwritable_typedef.h"

foo_typedef p = ((void *)0);

// To make sure we are testing what we want to test, make sure bar is rewritten
// as if foo_typedef is unconstrained. If foo_typedef were constrained, we'd
// expect bar to be written differently.
int *bar(void) {
  // CHECK: _Ptr<int> bar(void) _Checked {
  return p;
  // Make sure 3C isn't inserting a cast or something clever like that.
  // CHECK: return p;
}
