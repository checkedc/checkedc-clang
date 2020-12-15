//
// This is a regression test case for
// https://github.com/Microsoft/checkedc-clang/issues/484
//
// In checked scopes, we rewrite function types with bounds-safe interfaces to
// be fully checked. We need to rewrite declarations of parameters and uses of
// parameters that have bounds-safe interfaces.
//
// The following example illustrates the problem, if we don't do the rewrite.
// We define 3 C typedefs:
// 1. The first one defines an unchecked function pointer with a bounds-safe
// interface on a parameter.
// 2. The second one defines the bounds-safe interface type for the first
// typedef's function pointer type.
// 3. The third one defines a fully checked version.
//
// If we don't rewrite the uses of parameters, the function types
// don't match and we get a type checking error.
// RUN: %clang -Wno-check-bounds-decls-checked-scope -c -o %t %s

#pragma CHECKED_SCOPE ON

typedef int (*callback_fn3)(int *a : count(n), int n);
typedef  _Ptr<int (int *a : bounds(a, a + n), int n)> bsi_callback_fn3;
typedef  _Ptr<int (_Array_ptr<int> a : bounds(a, a + n), int n)> checked_callback_fn3;

_Checked callback_fn3 return_function_pointer(void) : itype(bsi_callback_fn3);

_Checked void test_function_pointer_return(void) {
   checked_callback_fn3 fn = return_function_pointer();
}



