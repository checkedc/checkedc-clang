// This tests compiling using Pre-Compiled Headers (PCH)
// To do so, we compile and verify this file against a header twice, once directly, and once via PCH
// If everything is working, both will succeed or fail together. If not, PCH is broken.
// PCH is one of the few places where AST Deserialization is used.

// Test this without PCH
// RUN: %clang_cc1 -fcheckedc-extension -include %S/pch.h -fsyntax-only -verify -verify-ignore-unexpected=note %s

// Test PCH (so we know our changes to AST deserialization haven't broken it)
// RUN: %clang_cc1 -fcheckedc-extension -emit-pch -o %t %S/pch.h
// RUN: %clang_cc1 -fcheckedc-extension -include-pch %t -fsyntax-only -verify -verify-ignore-unexpected=note %s

// Bounds Expressions on globals

// CountBounds
one_element_array arr1 = (int[1]){ 0 };

// NullaryBounds
null_array arr2 = (int[]){ 0 };

// RangeBounds
ranged_array arr3 = two_arr;

// InteropTypeBoundsAnnotation
int seven = 7;
integer_pointer seven_pointer1 = &seven;
_Ptr<int> seven_pointer2 = &seven;

// Bounds Expressions on functions
int accepts_singleton(_Array_ptr<int> one_arr : count(1)) {
  return one_arr[0];
}
void uses_accepts_singleton(void) {
  _Array_ptr<int> singleton : count(1) = (int[1]) { 0 };
  int x = accepts_singleton(singleton);
}

// RangeBounds + PositionalParamater
int sum_array(_Array_ptr<int> start : bounds(start, end), _Array_ptr<int> end) {
  return 0;
}
void uses_sum_array(void) {
  _Array_ptr<int> pair : count(2) = (int[2]) { 0,1 };
  int x = sum_array(pair, pair+1);
}

// PositionalParameter
int str_last(int len, _Array_ptr<char> str : count(len)) {
  return 0;
}
void uses_str_last(void) {
  _Array_ptr<char> str : count(3) = (char[3]){ 'a', 'b', 'c' };
  int last = str_last(3, str);
}

// InteropTypeBoundsAnnotation
int int_val(int *ptr : itype(_Ptr<int>)) {
  return 0;
}
void uses_int_val(void) {
  int i = 3;
  _Ptr<int> i_star = &i;
  int i2 = int_val(i_star);
}

// Dropping bounds errors
int accepts_pair(_Array_ptr<int> two_arr) { // expected-error{{function redeclaration dropped bounds for parameter}}
  return 3;
}