// Like canwrite_constraints_unimplemented.c but tests the third former base dir
// matching bug from
// https://github.com/correctcomputation/checkedc-clang/issues/327: symlinks.
// See the comments in canwrite_constraints_unimplemented.c.

// We can't assume we can make symlinks on Windows.
// UNSUPPORTED: system-windows

// Set up the following structure:
//
// %t.base/
//   canwrite_constraints_symlink.c : copy of this file
//   base_subdir_partial_defn.h -> clang/test/3C/base_subdir_partial_defn.h
//
// RUN: rm -rf %t*
// RUN: mkdir %t.base
// RUN: cp %s %t.base/canwrite_constraints_symlink.c
// RUN: ln -s %S/../base_subdir_partial_defn.h %t.base/base_subdir_partial_defn.h

// Now 3C should know that it can't write to base_subdir_partial_defn.h because
// the symlink goes out of the base dir.
//
// RUN: cd %t.base
// RUN: 3c -addcr canwrite_constraints_symlink.c -- -Xclang -verify

// expected-error@base_subdir_partial_defn.h:1 {{3C internal error: 3C generated changes to this file even though it is not allowed to write to the file}}
// expected-note@*:* {{-dump-unwritable-changes}}
// expected-note@*:* {{-allow-unwritable-changes}}

void
#include "base_subdir_partial_defn.h"
