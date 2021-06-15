// This tests compiling a CheckedC file using Pre-Compiled Headers (PCH)
// To do so, we compile and verify this file against a header twice, once directly, and once via PCH
// If everything is working, both will succeed or fail together. If not, PCH is broken.
// PCH is one of the few places where AST Deserialization is used.

// Test this without PCH
// RUN: %clang_cc1 -fcheckedc-extension -include %S/pch.h -fsyntax-only -verify -verify-ignore-unexpected=note %s

// Test PCH (so we know our changes to AST deserialization haven't broken it)
// RUN: %clang_cc1 -fcheckedc-extension -emit-pch -o %t %S/pch.h
// RUN: %clang_cc1 -fcheckedc-extension -include-pch %t -fsyntax-only -verify -verify-ignore-unexpected=note %s


//
// Basic Checked C Types
//

_Ptr<int> p1;
_Ptr<int> p1 = 0;
int* p1; // expected-error {{redefinition of 'p1' with a different type}}

_Array_ptr<int> p2;
_Array_ptr<int> p2 = 0;
int* p2; // expected-error {{redefinition of 'p2' with a different type}}

int arr1 _Checked[];
int arr1 _Checked[] = { 1, 2, 3 };
int* arr1; // expected-error {{redefinition of 'arr1' with a different type}}

int arr2 _Checked[1];
int arr2 _Checked[1] = { 1 };
int arr2[]; // expected-error {{redefinition of 'arr2' with a different type}}
int* arr2; // expected-error {{redefinition of 'arr2' with a different type}}

int arr3 _Checked[][1];
int arr3 _Checked[][1] = { { 1 } };
int arr3[][1]; // expected-error {{redefinition of 'arr3' with a different type}}
int** arr3; // expected-error {{redefinition of 'arr3' with a different type}}

_Array_ptr<char> str;
_Array_ptr<char> str = "foo bar";
char* str; // expected-error {{redefinition of 'str' with a different type}}

_Ptr<int(_Array_ptr<int> arr : count(i), int i)> f1;
_Ptr<int(_Array_ptr<int> arr : count(i), int i)> f1 = 0;
_Ptr<int(_Array_ptr<int> arr, int i)> f1; // expected-error {{redefinition of 'f1' with a different type}}
_Ptr<int(_Array_ptr<int>, int)> f1; // expected-error {{redefinition of 'f1' with a different type}}
int(*f1)(int* arr, int i); // expected-error {{redefinition of 'f1' with a different type}}

//
// Bounds Expressions on global variables
//

// CountBounds
_Array_ptr<int> one_arr : count(1 + 1); // expected-error{{variable redeclaration has conflicting bounds}}
_Array_ptr<int> one_arr : count(1);

// Byte Count
_Array_ptr<int> byte_arr : byte_count(sizeof(int));

// NullaryBounds
_Array_ptr<int> null_arr : count(1); // expected-error{{variable redeclaration has conflicting bounds}}
_Array_ptr<int> null_arr : bounds(unknown);

// RangeBounds
int two_arr[2];
_Array_ptr<int> ranged_arr : bounds(&one_arr, &one_arr + 1); //expected-error{{variable redeclaration has conflicting bounds}}
_Array_ptr<int> ranged_arr : bounds(&two_arr, &two_arr + 1);

// InteropTypeBoundsAnnotation
int* int_ptr : itype(_Array_ptr<int>); // expected-error{{variable redeclaration has conflicting interop type}}
int* int_ptr : itype(_Ptr<int>);
int* int_ptr;

//
// Bounds Expressions on functions
//

// CountBounds
int count_fn(_Array_ptr<int> arr : count(2)); // expected-error{{function redeclaration has conflicting parameter bounds}}
int count_fn(_Array_ptr<int> arr : count(1));
_Array_ptr<int> count_fn2(void) : count(2); // expected-error{{function redeclaration has conflicting return bounds}}
_Array_ptr<int> count_fn2(void) : count(1);

// Byte Count
int byte_count_fn(_Array_ptr<int> arr : byte_count(sizeof(int) + 1)); // expected-error{{function redeclaration has conflicting parameter bounds}}
int byte_count_fn(_Array_ptr<int> arr : byte_count(sizeof(int)));
_Array_ptr<int> byte_count_fn2(void) : byte_count(sizeof(int) + 1); // expected-error{{function redeclaration has conflicting return bounds}}
_Array_ptr<int> byte_count_fn2(void) : byte_count(sizeof(int));

// NullaryBounds
int unknown_fn(_Array_ptr<int> null_arr : count(1)); // expected-error{{function redeclaration has conflicting parameter bounds}}
int unknown_fn(_Array_ptr<int> null_arr : bounds(unknown));
_Array_ptr<int> unknown_fn2(void) : count(1); // expected-error{{function redeclaration has conflicting return bounds}}
_Array_ptr<int> unknown_fn2(void) : bounds(unknown);

// RangeBounds + PositionalParamater
int range_fn(_Array_ptr<int> start : bounds(start, end - 1), _Array_ptr<int> end); // expected-error{{function redeclaration has conflicting parameter bounds}}
int range_fn(_Array_ptr<int> start : bounds(start, end), _Array_ptr<int> end);
_Array_ptr<int> range_fn2(_Array_ptr<int> start) : bounds(start, start + 1); // expected-error{{function redeclaration has conflicting return bounds}}
_Array_ptr<int> range_fn2(_Array_ptr<int> start) : bounds(start, start);

// CountBounds + PositionalParameter
int pos_fn(int len, _Array_ptr<char> str : count(len + 1)); // expected-error{{function redeclaration has conflicting parameter bounds}}
int pos_fn(int len, _Array_ptr<char> str : count(len));
_Array_ptr<int> pos_fn2(int len) : count(len + 1); // expected-error{{function redeclaration has conflicting return bounds}}
_Array_ptr<int> pos_fn2(int len) : count(len);

// InteropTypeBoundsAnnotation
int int_val(int *ptr : itype(_Array_ptr<int>)); // expected-error{{function redeclaration has conflicting parameter interop type}}
int int_val(int *ptr : itype(_Ptr<int>));
int int_val(int *ptr);
int* int_val2(void) : itype(_Array_ptr<int>); // expected-error{{function redeclaration has conflicting return interop type}}
int* int_val2(void) : itype(_Ptr<int>);
int* int_val2(void);


//
// Bounds Expressions on Struct Members
//

// CountBounds
struct S1;
void uses_S1(struct S1 data) {
  _Array_ptr<int> arr : count(5) = data.arr;
}

// Byte Count 
struct S2;
void uses_S2(struct S2 data) {
  _Array_ptr<int> arr : byte_count(sizeof(int) * 5) = data.arr;
}

// NullaryBounds
struct S3;
void uses_S3(struct S3 data) {
  _Array_ptr<int> arr : bounds(unknown) = data.arr;
}

// RangeBounds
struct S4;
void uses_S4(struct S4 data) {
  _Array_ptr<long> arr : bounds(data.arr, data.arr + 5) = data.arr;
}

// InteropTypeBoundsAnnotation
struct S5;
void uses_S5(struct S5 data) {
  // Tests assigning a `_Ptr<int>` into an `int* : itype(_Ptr<int>)`
  // This should be valid, according to the spec. 
  int i = 3;
  _Ptr<int> ip = &i;
  data.i = ip;
}