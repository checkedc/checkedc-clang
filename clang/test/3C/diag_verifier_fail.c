// TODO: refactor this test
// https://github.com/correctcomputation/checkedc-clang/issues/503
// XFAIL: *

// Test that the diagnostic verifier is functioning correctly in 3c, because if
// it isn't, all the other regression tests that use the diagnostic verifier may
// not be able to catch the problems they are supposed to catch.
//
// This is exactly the same as diag_verifier_pass.c except that the warning
// message we expect is deliberately wrong, so the diagnostic verifier should
// fail.

// RUN: rm -rf %t*
// RUN: not 3c -base-dir=%S -extra-arg="-Wno-everything" -verify -warn-root-cause %s -- 2>%t.stderr
// RUN: grep -q "error: 'warning' diagnostics expected but not seen:" %t.stderr
// RUN: grep -q "error: 'warning' diagnostics seen but not expected:" %t.stderr

// Example warning borrowed from root_cause.c .
void *x; // expected-warning {{Default oops* type}}
