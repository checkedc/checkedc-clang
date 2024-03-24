//
// These are regression tests cases for 
// https://github.com/Microsoft/checkedc-clang/issues/242.
//
// The tests check that there are not duplicate error messages when a cast to _Ptr
// is incorrect. The incorrect bounds should not propagate through the cast to _Ptr.
//
// RUN: %clang -cc1 -verify -fcheckedc-extension %s

// Test explicit cast.
void f(int *p) {
  // Test explicit cast where bounds are expected for the entire initializing expression.
  _Array_ptr<int> q : count(1) = (_Ptr<int>)(p); // expected-error {{expression has unknown bounds, cast to ptr<T> expects source to have bounds}}
  // Test explicit cast.
  _Ptr<int> r = (_Ptr<int>)(p);    // expected-error {{expression has unknown bounds, cast to ptr<T> expects source to have bounds}}
  // Test implicit cast.
  _Ptr<int> s = p;                 // expected-error {{expression has unknown bounds, cast to ptr<T> expects source to have bounds}}
  _Array_ptr<int> t : count(1) = 0;
  // Test explicit cast involving assignment;
  t = (_Ptr<int>)(p);              // expected-error {{expression has unknown bounds, cast to ptr<T> expects source to have bounds}}
  _Ptr<int> u = 0;
  // Test implicit cast involving assignment.
  u = p;                           // expected-error {{expression has unknown bounds, cast to ptr<T> expects source to have bounds}}
}

