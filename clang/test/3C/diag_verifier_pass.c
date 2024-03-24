// Test that the diagnostic verifier is functioning correctly in 3c, because if
// it isn't, all the other regression tests that use the diagnostic verifier may
// not be able to catch the problems they are supposed to catch. This goes with
// diag_verifier_fail.c.
//
// If 3c correctly exits nonzero when there is an error diagnostic (as tested by
// stdout_mode_write_other.c) and the diagnostic verifier is not active at all,
// then we would expect all the tests with expected errors to actually report
// those errors and fail, so diag_verifier_{pass,fail}.c only add coverage for
// unusual problems with the diagnostic verifier that we haven't seen yet.

// RUN: 3c -base-dir=%S -warn-root-cause %s -- -Xclang -verify -Wno-everything

// Example warning borrowed from root_cause.c .
void *x; // expected-warning {{Default void* type}}
