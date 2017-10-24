// Used with the pch.c test

//
// Basic Checked C Types
//

_Ptr<int> p1;

_Array_ptr<int> p2;

int arr1 _Checked[];
int arr2 _Checked[1];
int arr3 _Checked[][1];

_Array_ptr<char> str;

_Ptr<int(_Array_ptr<int> arr : count(i), int i)> f1;

//
// Bounds Expressions on global variables
//

// CountBounds
_Array_ptr<int> one_arr : count(1);

// Byte Count
_Array_ptr<int> byte_arr : byte_count(sizeof(int));

// NullaryBounds
_Array_ptr<int> null_arr : bounds(unknown);

// RangeBounds
int two_arr[2];
_Array_ptr<int> ranged_arr : bounds(&two_arr, &two_arr + 1);

// InteropTypeBoundsAnnotation
int* int_ptr : itype(_Ptr<int>);

//
// Bounds Expressions on functions
//

// CountBounds
int count_fn(_Array_ptr<int> arr : count(1));
_Array_ptr<int> count_fn2(void) : count(1);

// Byte Count
int byte_count_fn(_Array_ptr<int> arr : byte_count(sizeof(int)));
_Array_ptr<int> byte_count_fn2(void) : byte_count(sizeof(int));

// NullaryBounds
int unknown_fn(_Array_ptr<int> null_arr : bounds(unknown));
_Array_ptr<int> unknown_fn2(void) : bounds(unknown);

// RangeBounds + PositionalParameter
int range_fn(_Array_ptr<int> start : bounds(start, end), _Array_ptr<int> end);
_Array_ptr<int> range_fn2(_Array_ptr<int> start) : bounds(start, start);

// CountBounds +  PositionalParameter
int pos_fn(int len, _Array_ptr<char> str : count(len));
_Array_ptr<int> pos_fn2(int len) : count(len);

// InteropTypeBoundsAnnotation
int int_val(int *ptr : itype(_Ptr<int>));
int* int_val2(void) : itype(_Ptr<int>);

//
// Bounds Expressions on Struct Members
//

// CountBounds
struct S1 {
  _Array_ptr<int> arr : count(5);
};

// Byte Count
struct S2 {
  _Array_ptr<int> arr : byte_count(sizeof(int) * 5);
};

// NullaryBounds
struct S3 {
  _Array_ptr<int> arr : bounds(unknown);
};

// RangeBounds
struct S4 {
  _Array_ptr<long> arr : bounds(arr, arr + 5);
};

// InteropTypeBoundsAnnotation
struct S5 {
  int* i : itype(_Ptr<int>);
};
