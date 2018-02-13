// Tests for checking that bounds declarations involving bounds-safe interfaces
// hold after assignments to variables and initialization of variables.  Because the static
// checker is mostly unimplemented, we only issue warnings when bounds declarations
// cannot be provided to hold.
//
// RUN: %clang -cc1 -fcheckedc-extension -Wcheck-bounds-decls -verify %s


// Test uses of incomplete types

struct S;
extern void test_f1(const void* p_ptr : byte_count(1));

int f1(_Ptr<struct S> p) {
  // TODO: Github Checked C repo issue #422: Extend constant-sized ranges to cover Ptr to an incomplete type
  test_f1(p); // expected-warning {{cannot prove argument meets declared bounds for 1st parameter}} \
               // expected-note {{(expanded) expected argument bounds are 'bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + 1)'}} \
               // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<struct S>)p, (_Array_ptr<struct S>)p + 1)'}}
  return 0;
}

/*
int f2(_Ptr<void> p) {
  test_f1(p);
  return 0;
}

#pragma BOUNDS_CHECKED ON

extern void test_f3(const void* p_ptr : byte_count(1)); 

int f3(_Ptr<struct S> p) {
  // TODO: Github Checked C repo issue #422: Extend constant-sized ranges to cover Ptr to an incomplete type
  test_f3(p); // expected-warning {{cannot prove argument meets declared bounds for 1st parameter}} \
              // expected-note {{(expanded) expected argument bounds are 'bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + 1)'}} \
              // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<struct S>)p, (_Array_ptr<struct S>)p + 1)'}}
  return 0;
}
*/
