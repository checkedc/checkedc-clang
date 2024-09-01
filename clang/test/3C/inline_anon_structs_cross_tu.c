// Regression tests for two bugs seen during the development of
// https://github.com/correctcomputation/checkedc-clang/pull/657 with multiple
// translation units: some information about automatically named TagDecls was
// not propagated across translation units.
//
// Our translation units are inline_anon_structs.c and
// inline_anon_structs_cross_tu.c; the latter `#include`s the former and has
// some code of its own. So inline_anon_structs.c effectively plays the role of
// a shared header file, but we just use the .c file instead of moving the code
// to a .h file and making another .c file that does nothing but `#include` the
// .h file.
//
// We test 3C on both orders of the translation units. Remember that when the
// same file is rewritten as part of multiple translation units, the last
// translation unit wins
// (https://github.com/correctcomputation/checkedc-clang/issues/374#issuecomment-804283984).
// So the "inline_anon_structs.c, inline_anon_structs_cross_tu.c" order should
// catch most problems that occur when 3C makes the decisions in one translation
// unit but does the rewriting in another. We still test the other order for
// completeness.

// RUN: rm -rf %t*

// Tests of the "inline_anon_structs.c, inline_anon_structs_cross_tu.c" order
// (called "AB" for short).
//
// Note about check prefixes: We currently don't bother with a Cartesian product
// of {ALL,NOALL} x {AB,BA} because we don't have any tests whose results depend
// on both variables. We do pass CHECK_NOALL and CHECK_ALL to
// inline_anon_structs_cross_tu.c for uniformity, even though that file
// currently doesn't use either.
//
// RUN: 3c -base-dir=%S -addcr -alltypes -output-dir=%t.checkedALL_AB %S/inline_anon_structs.c %s --
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK","CHECK_AB" --input-file %t.checkedALL_AB/inline_anon_structs.c %S/inline_anon_structs.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK","CHECK_AB" --input-file %t.checkedALL_AB/inline_anon_structs_cross_tu.c %s
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checkedNOALL_AB %S/inline_anon_structs.c %s --
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK","CHECK_AB" --input-file %t.checkedNOALL_AB/inline_anon_structs.c %S/inline_anon_structs.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK","CHECK_AB" --input-file %t.checkedNOALL_AB/inline_anon_structs_cross_tu.c %s

// Tests of the "inline_anon_structs_cross_tu.c, inline_anon_structs.c" order
// (called "BA").
//
// RUN: 3c -base-dir=%S -addcr -alltypes -output-dir=%t.checkedALL_BA  %s %S/inline_anon_structs.c --
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK","CHECK_BA" --input-file %t.checkedALL_BA/inline_anon_structs.c %S/inline_anon_structs.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK","CHECK_BA" --input-file %t.checkedALL_BA/inline_anon_structs_cross_tu.c %s
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checkedNOALL_BA %s %S/inline_anon_structs.c --
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK","CHECK_BA" --input-file %t.checkedNOALL_BA/inline_anon_structs.c %S/inline_anon_structs.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK","CHECK_BA" --input-file %t.checkedNOALL_BA/inline_anon_structs_cross_tu.c %s

void second_tu_fn(void) {
  // In the AB order, the global `cross_tu_numbering_test` variable in
  // inline_anon_structs.c will be seen first and its struct will be named
  // cross_tu_numbering_test_struct_1, and this struct will be named
  // cross_tu_numbering_test_struct_2. In the BA order, this code is seen before
  // the `#include "inline_anon_structs.c"`, so this struct will take the
  // cross_tu_numbering_test_struct_1 name and the inline_anon_structs.c one
  // will be cross_tu_numbering_test_struct_2.
  //
  // Note that in order to have two different variables with the same name (in
  // order to produce a collision in struct names), we have to put the variables
  // in different scopes: in this case, global and function-local.
  struct { int y; } *cross_tu_numbering_test;
  //CHECK_AB:      struct cross_tu_numbering_test_struct_2 { int y; };
  //CHECK_AB-NEXT: _Ptr<struct cross_tu_numbering_test_struct_2> cross_tu_numbering_test = ((void *)0);
  //CHECK_BA:      struct cross_tu_numbering_test_struct_1 { int y; };
  //CHECK_BA-NEXT: _Ptr<struct cross_tu_numbering_test_struct_1> cross_tu_numbering_test = ((void *)0);
}

#include "inline_anon_structs.c"
