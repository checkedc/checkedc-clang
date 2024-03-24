// An example of a case that is not handled by the canWrite constraints code and
// causes 3C to generate a change to an unwritable file. Test that 3C generates
// an error diagnostic.
// (https://github.com/correctcomputation/checkedc-clang/issues/387)

// RUN: cd %S
// RUN: 3c -addcr %s -- -Xclang -verify

// expected-error@../base_subdir_partial_defn.h:1 {{3C internal error: 3C generated changes to this file even though it is not allowed to write to the file}}
// expected-note@*:* {{-dump-unwritable-changes}}
// expected-note@*:* {{-allow-unwritable-changes}}

// The "../base_subdir_partial_defn.h" path is testing two former base dir
// matching bugs from
// https://github.com/correctcomputation/checkedc-clang/issues/327:
//
// 1. 3C canonicalizes ".." path elements. Otherwise, it would think ".." is a
//    subdirectory of the current directory.
//
// 2. 3C compares entire path components and knows that
//    base_subdir_partial_defn.h is not under base_subdir.
//
// It would be valid to test these cases in combination with either code that is
// correctly constrained or code for which constraining is unhandled. We choose
// the latter in order to keep all the "weird" stuff in this test file and leave
// canwrite_constraints.c sane.

// Since the declaration of foo begins with the "void" in this (writable) file,
// 3C currently assumes the whole declaration is writable. Then 3C rewrites the
// declaration of the parameter "x", which is wholly within
// ../base_subdir_partial_defn.h; the rewriter seems to handle this with no
// trouble. But then the proposed change to an unwritable file trips the error
// we are testing.
//
// Changing 3C to handle such contrived preprocessor nonsense correctly is a low
// priority, so this test hopefully won't have to be changed for a while, if
// ever.

void
#include "../base_subdir_partial_defn.h"
