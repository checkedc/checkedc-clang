// TODO: refactor this test
// https://github.com/correctcomputation/checkedc-clang/issues/503
// XFAIL: *

// Like the base_subdir/canwrite_constraints_unimplemented test except that
// writing base_subdir_partial_defn.h is not restricted by the base dir, so we
// reach the stdout mode restriction.

// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -verify %s --

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
