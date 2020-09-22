// Tests for checking that bounds declarations involving bounds-safe interfaces
// hold after assignments to variables and initialization of variables.  Because the static
// checker is being implemented, we only issue warnings when bounds declarations
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

int f2(_Ptr<void> p) {
  test_f1(p);
  return 0;
}

#pragma CHECKED_SCOPE ON

extern void test_f3(const void* p_ptr : byte_count(1));

int f3(_Ptr<struct S> p) {
  // TODO: Github Checked C repo issue #422: Extend constant-sized ranges to cover Ptr to an incomplete type
  test_f3(p); // expected-warning {{cannot prove argument meets declared bounds for 1st parameter}} \
              // expected-note {{(expanded) expected argument bounds are 'bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + 1)'}} \
              // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<struct S>)p, (_Array_ptr<struct S>)p + 1)'}}
  return 0;
}

#pragma CHECKED_SCOPE OFF

//
// Test use of return bounds-safe interfaces
//

int *test_f10(unsigned c) : count(c);

_Array_ptr<int> f11(unsigned num) {
  _Array_ptr<int> p : count(num) = test_f10(num);
  return p;
}

_Array_ptr<int> f12(unsigned num1, unsigned num2){
  _Array_ptr<int> p : count(num1) = test_f10(num2); // expected-warning {{cannot prove declared bounds for 'p' are valid after initialization}} \
                                                    // expected-note {{(expanded) declared bounds are 'bounds(p, p + num1)'}} \
                                                    // expected-note {{(expanded) inferred bounds are 'bounds(value of test_f10(num2), value of test_f10(num2) + num2)'}}
  return p;
}

int *test_f13(unsigned c) : byte_count(c);

_Array_ptr<int> f14(unsigned num) {
  _Array_ptr<int> p : byte_count(num) = test_f13(num);
  return p;
}

_Array_ptr<int> f15(unsigned num1, unsigned num2){
  _Array_ptr<int> p : byte_count(num1) = test_f13(num2); // expected-warning {{cannot prove declared bounds for 'p' are valid after initialization}} \
                                                         // expected-note {{(expanded) declared bounds are 'bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + num1)'}} \
                                                         // expected-note {{inferred bounds are 'bounds((_Array_ptr<char>)value of test_f13(num2), (_Array_ptr<char>)value of test_f13(num2) + num2)'}}
  return p;
}

int *test_f16(unsigned c) : itype(_Ptr<int>);

_Array_ptr<int> f17(unsigned num) {
  _Array_ptr<int> p : count(1) = test_f16(num);
  return p;
}

_Array_ptr<int> f18(unsigned num) {
  _Array_ptr<int> p : count(2) = test_f16(num);         // expected-error {{declared bounds for 'p' are invalid after initialization}} \
                                                        // expected-note {{destination bounds are wider than the source bounds}} \
                                                        // expected-note {{destination upper bound is above source upper bound}} \
                                                        // expected-note {{(expanded) declared bounds are 'bounds(p, p + 2)'}} \
                                                        // expected-note {{(expanded) inferred bounds are 'bounds(value of test_f16(num), value of test_f16(num) + 1)'}}
  return p;
}


