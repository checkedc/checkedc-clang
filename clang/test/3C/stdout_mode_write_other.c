// Like the base_subdir/canwrite_constraints_unimplemented test except that
// writing base_subdir_partial_defn.h is not restricted by the base dir, so we
// reach the stdout mode restriction.

// RUN: 3c -base-dir=%S -addcr -verify %s --

// expected-error@base_subdir_partial_defn.h:1 {{3C generated changes to this file, which is under the base dir but is not the main file and thus cannot be written in stdout mode}}
// expected-note@*:* {{-dump-unwritable-changes}}
// expected-note@*:* {{-allow-unwritable-changes}}

void
#include "base_subdir_partial_defn.h"
