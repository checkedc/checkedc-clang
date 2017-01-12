// Used with the pch.c test

// Bounds Expressions on globals

// CountBounds
_Array_ptr<int> one_arr : count(1);
typedef typeof(one_arr) one_element_array;

// NullaryBounds
_Array_ptr<int> null_arr : bounds(none);
typedef typeof(null_arr) null_array;

// RangeBounds
int two_arr[2] = { 0, 0 };
_Array_ptr<int> ranged_arr : bounds(&two_arr, &two_arr + 1);
typedef typeof(ranged_arr) ranged_array;

// InteropTypeBoundsAnnotation
int* int_ptr : itype(_Ptr<int>);
typedef typeof(int_ptr) integer_pointer;


// Bounds Expressions on functions
int accepts_singleton(_Array_ptr<int> one_arr : count(1));

// NullaryBounds
int accepts_null(_Array_ptr<int> null_arr : bounds(none));

// RangeBounds + PositionalParameter
int sum_array(_Array_ptr<int> start : bounds(start, end) , _Array_ptr<int> end);

// PositionalParameter
int str_last(int len, _Array_ptr<char> str : count(len));

// InteropTypeBoundsAnnotation
int int_val(int *ptr : itype(_Ptr<int>));

// dropping bounds errors
int accepts_pair(_Array_ptr<int> two_arr : count(2));