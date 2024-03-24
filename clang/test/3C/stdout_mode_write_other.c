// Like the base_subdir/canwrite_constraints_unimplemented test except that
// writing base_subdir_partial_defn.h is not restricted by the base dir, so we
// reach the stdout mode restriction.
//
// Since this is currently the simplest 3C regression test that tests an error
// diagnostic, we also use it to test some things about error diagnostics in
// general.

// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr %s -- -Xclang -verify 2>%t.stderr
// RUN: grep -q 'Exiting successfully because the failure was due solely to expected error diagnostics and diagnostic verification succeeded' %t.stderr

// Same as above, except instead of using the diagnostic verifier, manually test
// that 3c exits nonzero and prints the error to stderr. This tests that 3c
// handles diagnostics properly.
//
// RUN: not 3c -base-dir=%S -addcr %s -- 2>%t.stderr
// RUN: grep -q 'error: 3C generated changes to this file, which is under the base dir but is not the main file and thus cannot be written in stdout mode' %t.stderr

// expected-error@base_subdir_partial_defn.h:1 {{3C generated changes to this file, which is under the base dir but is not the main file and thus cannot be written in stdout mode}}
// expected-note@*:* {{-dump-unwritable-changes}}
// expected-note@*:* {{-allow-unwritable-changes}}

void
#include "base_subdir_partial_defn.h"
