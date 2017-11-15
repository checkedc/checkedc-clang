// Test for checked scopes that bounds declaration checking always
// produces warnings about bounds declarations that are not provably
// true or false.
//
// RUN: %clang -cc1 -fcheckedc-extension -verify %s

#pragma BOUNDS_CHECKED ON

#include "bounds-decl-checking.c"

