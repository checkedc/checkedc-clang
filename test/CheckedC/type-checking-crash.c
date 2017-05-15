// Checks for crashes during type-checking error.
// This was caused by an lvalue assert in SemaBounds.cpp
// The goal of this test is to make sure we're seeing 
// type-checking errors, but not get any crashes.
// The llvm-lit test will fail the test if the compiler
// crashes because of assert failure (This has been tested)
// I wanted to use FileCheck to specifically look for
// "assert failed" output but for some reason FileCheck 
// refuses to work. So I had to use this method.
// RUN: %clang_cc1 -fcheckedc-extension -verify %s

struct S {
	char arr _Checked[10];
	int data;
};

void struct_int_array_type_checking_crash_test_1(int i) {
	struct S s = { { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }, 10 };
	s->arr[i] = 1; // expected-error{{expression has no bounds}}	// expected-error{{member reference type 'struct S' is not a pointer; did you mean to use '.'?}} 
}

void struct_int_array_type_checking_crash_test_2(int i) {
	struct S s = { { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }, 10 };
	s.arr[i] = 1;
}

void struct_int_array_type_checking_crash_test_3(int i) {
	struct S s = { { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }, 10 };
	(&s)->arr[i] = 1;
}

void struct_int_array_type_checking_crash_test_4(int i) {
	struct S s = { { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }, 10 };
	(&s).arr[i] = 1;// expected-error{{member reference type 'struct S *' is a pointer; did you mean to use '->'?}}
}