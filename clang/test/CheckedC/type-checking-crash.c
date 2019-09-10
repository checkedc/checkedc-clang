// Tests to make sure the Checked C code does not crash the compiler when it
// encounters an incorrectly constructed AST caused by type checking error.
//
// More specifically, take a look at the four test cases below. First of all,
// even when clang encounters a type checking error, it does its best to create
// the entire AST, before it exits with an error message.
// Struct_int_array_type_checking_crash_test_1 creates an invalid AST, where
// "arr" is not an lvalue, although it should be an lvalue. Interestingly, for
// the other three test cases, the constructed AST has "arr" to be an lvalue.
// This test makes sure that no matter the case, the Checked C code will not
// exit the program with an exception.
//
// RUN: %clang_cc1 -fcheckedc-extension -verify %s

struct S {
  char arr _Checked[10];
  int data;
};

//====================================================================
// Accessing struct with "->" causes error and wrongly eliminates lvalue
//====================================================================

void struct_int_array_type_checking_crash_test_1(int i) {
  struct S s = { { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }, 10 };
  s->arr[i] = 1; //expected-error{{member reference type 'struct S' is not a pointer; did you mean to use '.'?}} //expected-error{{expression has unknown bounds}}
}

//====================================================================
// Correctly accessing struct.
//====================================================================

void struct_int_array_type_checking_crash_test_2(int i) {
  struct S s = { { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }, 10 };
  s.arr[i] = 1;
}

//====================================================================
// Correctly accessing struct pointer.
//====================================================================

void struct_int_array_type_checking_crash_test_3(int i) {
  struct S s = { { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }, 10 };
  (&s)->arr[i] = 1;
}

//====================================================================
// Accessing struct pointer with "." causes error but keeps lvalue
//====================================================================

void struct_int_array_type_checking_crash_test_4(int i) {
  struct S s = { { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }, 10 };
  (&s).arr[i] = 1;//expected-error{{member reference type 'struct S *' is a pointer; did you mean to use '->'?}}
}
