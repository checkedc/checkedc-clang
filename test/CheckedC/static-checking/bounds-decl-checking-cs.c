// Test for checked scopes that bounds declaration checking always
// produces warnings about bounds declarations that are not provably
// true or false.
//
// RUN: %clang_cc1 -fcheckedc-extension -verify %s

#pragma CHECKED_SCOPE ON

#include "bounds-decl-checking.c"

