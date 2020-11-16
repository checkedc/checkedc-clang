// Tests for checking that bounds declarations hold after assignments to
// variables and initialization of variables.  Because the static checker is
// mostly unimplemented, we only issue warnings when bounds declarations
// cannot be provided to hold.
//
// RUN: %clang_cc1 -fcheckedc-extension -Wcheck-bounds-decls -verify %s

#include <stddef.h>

// Initialization by null - no warning.
void f1(_Array_ptr<int> p : bounds(p, p + x), int x) {
    _Array_ptr<int> r : bounds(p, p + x) = 0;
}

// Initialization by expression with syntactically identical
// normalized bounds - no warning.
void f2(_Array_ptr<int> p : bounds(p, p + x), int x) {
  _Array_ptr<int> r : bounds(p, p + x) = p;
}

// Initialization of a pointer by an array with syntactically identical
// normalized bounds.
void f2a(void) {
  int a _Checked[10];
  _Array_ptr<int> r : bounds(a, a + 10) = a;
}

// Initialization by expression with bounds that are syntactically identical
// modulo expression equality implied by initialization - no warning.
void f3(_Array_ptr<int> p : bounds(p, p + x), int x) {
  _Array_ptr<int> r : count(x) = p;
}

// Initialization by an array with bounds that syntactically identical
// modulo expression equality implied by initialization - no warning.
void f3a(void) {
  int a _Checked[10];
  _Array_ptr<int> r : count(10) = a;
}


// Assignment of null - no warning.
void f4(_Array_ptr<int> p : count(x), int x) {
  _Array_ptr<int> r : bounds(p, p + x) = 0;
  r = 0;
}

// Assignment of expression with syntactically identical normalized bounds -
//- no warning.
void f5(_Array_ptr<int> p : count(x), int x) {
  _Array_ptr<int> r : bounds(p, p + x) = p;
  r = p;
}

// Assignment of expression with bounds that are syntactically identical
// modulo expression equality implied by assignment - no warning.
void f6(_Array_ptr<int> p : count(x), int x) {
  _Array_ptr<int> r : count(x) = 0;
  r = p;
}

// Parameter passing

void test_f1(_Array_ptr<int> arg : bounds(arg, arg + 1));

void test_f2(_Array_ptr<int> arg : count(1)) {
}

void test_calls(_Array_ptr<int> arg1 : count(1)) {
  // Passing null, no warning
  test_f1(0);

  _Array_ptr<int> t1 : bounds(t1, t1 + 1) = 0;
  // Syntactically identical bounds
  test_f1(t1);

  // Syntactically identical after expanding count
  test_f2(t1);
}

//
// Constant-sized ranges
//

// Initializers
void f20(_Array_ptr<int> p: count(5)) {
  _Array_ptr<int> q1 : bounds(p, p + 5) = p; // No error expected
  _Array_ptr<int> q2 : bounds(p + 0, p + 5) = p; // No error expected
  _Array_ptr<int> q3 : bounds(p, p + 4) = p; // No error expected
  _Array_ptr<int> q4 : bounds(p + 1, p + 4) = p; // No error expected
  _Array_ptr<int> r : bounds(p, p + 6) = p; // expected-error {{declared bounds for 'r' are invalid after initialization}} \
                                            // expected-note {{destination bounds are wider than the source bounds}} \
                                            // expected-note {{destination upper bound is above source upper bound}} \
                                            // expected-note {{(expanded) declared bounds are 'bounds(p, p + 6)'}} \
                                            // expected-note {{(expanded) inferred bounds are 'bounds(p, p + 5)'}}
  _Array_ptr<int> s : bounds(p - 1, p + 5) = p;  // expected-error {{declared bounds for 's' are invalid after initialization}} \
                                            // expected-note {{destination bounds are wider than the source bounds}} \
                                            // expected-note {{destination lower bound is below source lower bound}} \
                                            // expected-note {{(expanded) declared bounds are 'bounds(p - 1, p + 5)'}} \
                                            // expected-note {{(expanded) inferred bounds are 'bounds(p, p + 5)'}}
  _Array_ptr<int> t : bounds(p - 1, p + 6) = p;  // expected-error {{declared bounds for 't' are invalid after initialization}} \
                                            // expected-note {{destination bounds are wider than the source bounds}} \
                                            // expected-note {{destination lower bound is below source lower bound}} \
                                            // expected-note {{destination upper bound is above source upper bound}} \
                                            // expected-note {{(expanded) declared bounds are 'bounds(p - 1, p + 6)'}} \
                                            // expected-note {{(expanded) inferred bounds are 'bounds(p, p + 5)'}}
}

// Initialization of pointers using top-level arrays
int a0 _Checked[9] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
_Array_ptr<int> a1 : count(3) = a0;
_Array_ptr<int> a2 : count(11) = a0;   // expected-error {{declared bounds for 'a2' are invalid after initialization}} \
                                       // expected-note {{destination bounds are wider than the source bounds}} \
                                       // expected-note {{destination upper bound is above source upper bound}} \
                                       // expected-note {{(expanded) declared bounds are 'bounds(a2, a2 + 11)'}} \
                                       // expected-note {{(expanded) inferred bounds are 'bounds(a0, a0 + 9)'}}

// Assignments
void f21(_Array_ptr<int> p: count(5)) {
  _Array_ptr<int> q1 : bounds(p, p + 5) = 0;
   q1 = p; // No error expected
  _Array_ptr<int> q2 : bounds(p + 0, p + 5) = 0;
  q2 = p; // No error expected
  _Array_ptr<int> q3 : bounds(p, p + 4) = 0;
  q3 = p; // No error expected
  _Array_ptr<int> q4 : bounds(p + 1, p + 4) = 0;
  q4 = p; // No error expected
  _Array_ptr<int> r : bounds(p, p + 6) = 0; // expected-note {{(expanded) declared bounds are 'bounds(p, p + 6)'}}
  r = p; // expected-error {{declared bounds for 'r' are invalid after assignment}} \
         // expected-note {{destination bounds are wider than the source bounds}} \
         // expected-note {{destination upper bound is above source upper bound}} \
         // expected-note {{(expanded) inferred bounds are 'bounds(p, p + 5)'}}
  _Array_ptr<int> s : bounds(p - 1, p + 5) = 0; // expected-note {{(expanded) declared bounds are 'bounds(p - 1, p + 5)'}}
  s = p;  // expected-error {{declared bounds for 's' are invalid after assignment}} \
          // expected-note {{destination bounds are wider than the source bounds}} \
          // expected-note {{destination lower bound is below source lower bound}} \
          // expected-note {{(expanded) inferred bounds are 'bounds(p, p + 5)'}}
  _Array_ptr<int> t : bounds(p - 1, p + 6) = 0; // expected-note {{(expanded) declared bounds are 'bounds(p - 1, p + 6)'}}
  t = p;  // expected-error {{declared bounds for 't' are invalid after assignment}} \
          // expected-note {{destination bounds are wider than the source bounds}} \
          // expected-note {{destination lower bound is below source lower bound}} \
          // expected-note {{destination upper bound is above source upper bound}} \
          // expected-note {{(expanded) inferred bounds are 'bounds(p, p + 5)'}}
}

void constant_sized_bound1(_Array_ptr<int> p : count(5)) {
}

// Argument points into the middle of a range.
void constant_sized_bound2(_Array_ptr<int> p : bounds(p - 1, p + 4)) {
}

// TODO: variants of constant_sized bound where the argument points into the middle
// of a range.  We need reassociation of arithmetic expressions so that constant-folding
// works properly.  Example:
//  void constant_sized_bound2(_Array_ptr<int> p : bounds(p - 1, p + 5)
// If we have an argument variable v : bounds(v, v + 3), we'll ahve problems
// when v is substitu
void f22(_Array_ptr<int> q : count(5)) {
  constant_sized_bound1(q);  // No error_expected
  _Array_ptr<int> r : count(4) = 0;
  constant_sized_bound1(r);   // expected-error {{argument does not meet declared bounds for 1st parameter}} \
          // expected-note {{destination bounds are wider than the source bounds}} \
          // expected-note {{destination upper bound is above source upper bound}} \
          // expected-note {{(expanded) expected argument bounds are 'bounds(r, r + 5)'}} \
          // expected-note {{(expanded) inferred bounds are 'bounds(r, r + 4)'}}
  _Array_ptr<int> s : count(6) = 0;
  constant_sized_bound1(s);  // No error expected
  _Array_ptr<int> t1 : bounds(t1 + 1, t1 + 6) = 0;
  constant_sized_bound1(t1);  // expected-error {{argument does not meet declared bounds for 1st parameter}} \
          // expected-note {{destination lower bound is below source lower bound}} \
          // expected-note {{(expanded) expected argument bounds are 'bounds(t1, t1 + 5)'}} \
          // expected-note {{(expanded) inferred bounds are 'bounds(t1 + 1, t1 + 6)'}}
  _Array_ptr<int> t2 : bounds(t2 - 1, t2 + 4) = 0;
  constant_sized_bound1(t2);  // expected-error {{argument does not meet declared bounds for 1st parameter}} \
          // expected-note {{destination upper bound is above source upper bound}} \
          // expected-note {{(expanded) expected argument bounds are 'bounds(t2, t2 + 5)'}} \
          // expected-note {{(expanded) inferred bounds are 'bounds(t2 - 1, t2 + 4)'}}
  constant_sized_bound2(q); // expected-error {{argument does not meet declared bounds for 1st parameter}} \
          // expected-note {{destination lower bound is below source lower bound}} \
          // expected-note {{(expanded) expected argument bounds are 'bounds(q - 1, q + 4)'}} \
          // expected-note {{(expanded) inferred bounds are 'bounds(q, q + 5)'}}
  constant_sized_bound2(q + 1);  // expected-warning {{cannot prove argument meets declared bounds for 1st parameter}} \
                                 // expected-note {{(expanded) expected argument bounds are 'bounds(q + 1 - 1, q + 1 + 4)'}} \
                                 // expected-note {{(expanded) inferred bounds are 'bounds(q, q + 5)'}}
                                // TODO: no warning/error expected.  We need reassocation of arithmetic
                                // expressions to avoid the warning here

}

struct S1 {
  int x;
  int y;
};

struct S2 {
  int x;
  int y;
  int z;
  unsigned long long p;
};

void test_cast(_Ptr<struct S1> s1, _Ptr<struct S2> s2) {
  _Ptr<int> p = (_Ptr<int>)s1;
  _Ptr<char> cp = 0;
  cp = (_Ptr<char>) p;
  p = (_Ptr<int>) cp; // expected-error {{cast source bounds are too narrow for '_Ptr<int>'}} \
                      // expected-note {{target upper bound is above source upper bound}} \
                      // expected-note{{(expanded) required bounds are 'bounds((_Array_ptr<int>)cp, (_Array_ptr<int>)cp + 1)'}} \
                      // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)cp, (_Array_ptr<char>)cp + 1)'}} \
  _Ptr<struct S1> prefix = (_Ptr<struct S1>) s2;
  _Ptr<struct S2> suffix = (_Ptr<struct S2>) s1; // expected-error {{cast source bounds are too narrow for '_Ptr<struct S2>'}} \
                                                 // expected-note {{target upper bound is above source upper bound}} \
                                                 // expected-note {{(expanded) required bounds are 'bounds((_Array_ptr<struct S2>)s1, (_Array_ptr<struct S2>)s1 + 1)'}} \
                                                 // expected-note{{(expanded) inferred bounds are 'bounds((_Array_ptr<struct S1>)s1, (_Array_ptr<struct S1>)s1 + 1)'}}

}

_Ptr<void> test_void(void);

void test_ptr_void_cast(_Ptr<void> p) {
  _Ptr<int> ip = (_Ptr<int>) p; // expected-error {{cast source bounds are too narrow for '_Ptr<int>'}} \
                                // expected-note {{target upper bound is above source upper bound}} \
                                // expected-note {{(expanded) required bounds are 'bounds((_Array_ptr<int>)p, (_Array_ptr<int>)p + 1)'}} \
                                // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + 1)'}}
  _Ptr<struct S1> sp = (_Ptr<struct S1>) p; // expected-error {{cast source bounds are too narrow for '_Ptr<struct S1>'}} \
                                            // expected-note {{target upper bound is above source upper bound}} \
                                            // expected-note {{(expanded) required bounds are 'bounds((_Array_ptr<struct S1>)p, (_Array_ptr<struct S1>)p + 1)'}} \
                                            // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + 1)'}}
  _Ptr<char> cp = (_Ptr<char>) p;
  cp = (_Ptr<char>) test_void();
}



// Test casts between pointers to nt_arrays and pointers to arrays.
void test_nt_array_casts(void) {
  int nt_arr _Nt_checked[5];

  _Ptr<int _Nt_checked[5]> nt_parr1 = 0;
  // TODO: compiler needs to understand equivalence of value of the cast and nt_arr
  // nt_parr1 = (_Ptr<int _Nt_checked[5]>) &nt_arr;

  _Ptr<int _Nt_checked[6]> nt_parr2 = 0;
  nt_parr2 = (_Ptr<int _Nt_checked[6]>) &nt_arr; // expected-error {{cast source bounds are too narrow for '_Ptr<int _Nt_checked[6]>'}} \
                    // expected-note {{target upper bound is above source upper bound}} \
                    // expected-note {{(expanded) required bounds are 'bounds((_Array_ptr<int _Nt_checked[6]>)&nt_arr, (_Array_ptr<int _Nt_checked[6]>)&nt_arr + 1)'}} \
                    // expected-note {{(expanded) inferred bounds are 'bounds(nt_arr, nt_arr + 5)'}}

  _Ptr<int _Nt_checked[5]> nt_parr3 = 0;
  // TODO: compiler needs to understand equivalence of value of the cast and nt_arr
  // nt_parr3 = (_Ptr<int _Nt_checked[5]>) &nt_arr;
  _Ptr<int _Checked[4]> parr4 = 0;
  // TODO: compiler needs to understand equivalence of value of the cast and nt_arr
  // parr4 = (_Ptr<int _Checked[4]>) &nt_arr;

  _Ptr<int _Checked[5]> parr5 = 0;
  parr5 = (_Ptr<int _Checked[5]>) &nt_arr; // expected-error {{cast source bounds are too narrow for '_Ptr<int _Checked[5]>'}} \
                                           // expected-note {{target upper bound is above source upper bound}} \
                                           // expected-note {{(expanded) required bounds are 'bounds((_Array_ptr<int _Checked[5]>)&nt_arr, (_Array_ptr<int _Checked[5]>)&nt_arr + 1)'}} \
                                           // expected-note {{(expanded) inferred bounds are 'bounds(nt_arr, nt_arr + 4)}}

  _Ptr<int _Checked[5]> parr6 = 0;
  parr6 = (_Ptr<int _Checked[5]>) &nt_arr;  // expected-error {{cast source bounds are too narrow for '_Ptr<int _Checked[5]>'}} \
                    // expected-note {{target upper bound is above source upper bound}} \
                    // expected-note {{(expanded) required bounds are 'bounds((_Array_ptr<int _Checked[5]>)&nt_arr, (_Array_ptr<int _Checked[5]>)&nt_arr + 1)'}} \
                    // expected-note {{(expanded) inferred bounds are 'bounds(nt_arr, nt_arr + 4)'}}
}

void test_addition_commutativity(void) {
  _Array_ptr<int> p : bounds(p + 1, p + 5) = 0;
  _Array_ptr<int> q : bounds(1 + p, p + 5) = p;
  _Array_ptr<int> r : bounds(p + 1, 5 + p) = p;
}

// Test uses of incomplete types

struct S;
extern void test_f30(_Array_ptr<const void> p_ptr : byte_count(1));

int f30(_Ptr<struct S> p) {
  // TODO: Github Checked C repo issue #422: Extend constant-sized ranges to cover Ptr to an incomplete type
  test_f30(p); // expected-warning {{cannot prove argument meets declared bounds for 1st parameter}} \
               // expected-note {{(expanded) expected argument bounds are 'bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + 1)'}} \
               // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<struct S>)p, (_Array_ptr<struct S>)p + 1)'}}
  return 0;
}

int f31(_Ptr<void> p) {
  test_f30(p);
  return 0;
}

// The warning is not directly related to issue #599
// It is related to incomplete types.
_Array_ptr<struct S> f37_i(unsigned num) : count(num) {
  _Array_ptr<struct S> q : count(num) = 0;
  _Array_ptr<struct S> p : count(0) = q; // expected-warning {{cannot prove declared bounds for 'p' are valid after initialization}} \
                                         // expected-note {{(expanded) declared bounds are 'bounds(p, p + 0)'}} \
                                         // expected-note {{(expanded) inferred bounds are 'bounds(q, q + num)'}}
  return p;
}

_Array_ptr<int> f37(unsigned num) : count(num) {
  _Array_ptr<int> q : count(num) = 0;
  _Array_ptr<int> p : count(0) = q;
  return p;
}


_Nt_array_ptr<int> f37_n(unsigned num) : count(num) {
  _Nt_array_ptr<int> q : count(num) = 0;
  _Nt_array_ptr<int> p : count(0) = q;
  return p;
}

//
// Test uses of _Return_value.  Handling _Return_value during bounds
// declaration checking isn't implemented yet, so these uses are
// expected to generate warnings.
_Array_ptr<int> test_f32(unsigned c) : bounds(_Return_value, _Return_value + c);

#if 0
// TODO: fix expected error messages on Linux
_Array_ptr<int> f33(unsigned num) : count(num) {
  _Array_ptr<int> p : count(num) = test_f32(num); // dummy-expected-warning {{cannot prove declared bounds for 'p' are valid after initialization}} \
                                                  // dummy-expected-note {{(expanded) declared bounds are 'bounds(p, p + num)'}} \
                                                  // dummy-expected-note {{(expanded) inferred bounds are 'bounds(_Return_value, _Return_value + num)'}}
  return p;
}
#endif

_Array_ptr<void> test_f34(unsigned size) : bounds((_Array_ptr<char>) _Return_value, (_Array_ptr<char>) _Return_value + size);

#if 0
// TODO: fix expected error messages on Linux
_Array_ptr<int> f35(unsigned num) : count(num) {
  _Array_ptr<int> p : count(num) = test_f34(num * sizeof(int)); // dummy-expected-warning {{cannot prove declared bounds for 'p' are valid after initialization}} \
                                                                // dummy-expected-note {{(expanded) declared bounds are 'bounds(p, p + num)'}} \
                                                                // dummy-expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)_Return_value, (_Array_ptr<char>)_Return_value + num * sizeof(int))'}}
  return p;
}
#endif

_Array_ptr<void> test_f34(unsigned size) : bounds((_Array_ptr<char>) _Return_value, (_Array_ptr<char>) _Return_value + size);

#if 0
_Array_ptr<int> f36(unsigned num) : count(num) {
  _Array_ptr<int> p : count(num) = test_f34(num * sizeof(int));  // dummy-expected-warning {{cannot prove declared bounds for 'p' are valid after initialization}} \
                                                                 // dummy-expected-note {{(expanded) declared bounds are 'bounds(p, p + num)'}} \
                                                                 // dummy-expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)_Return_value, (_Array_ptr<char>)_Return_value + num * sizeof(int))'}}
  return p;
}
#endif


//
// Test use of return count bounds.
//

_Array_ptr<int> test_f50(unsigned c) : count(c);

_Array_ptr<int> f51(unsigned num) {
  _Array_ptr<int> p : count(num) = test_f50(num);
  return p;
}

_Array_ptr<int> f52(unsigned num1, unsigned num2){
  _Array_ptr<int> p : count(num1) = test_f50(num2); // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}} \
                                                    // expected-note {{the declared upper bounds use the variable 'num1' and there is no relational information involving 'num1' and any of the expressions used by the inferred upper bounds}} \
                                                    // expected-note {{the inferred upper bounds use the variable 'num2' and there is no relational information involving 'num2' and any of the expressions used by the declared upper bounds}} \
                                                    // expected-note {{(expanded) declared bounds are 'bounds(p, p + num1)'}} \
                                                    // expected-note {{(expanded) inferred bounds are 'bounds(value of test_f50(num2), value of test_f50(num2) + num2)'}}
  return p;
}

_Array_ptr<int> test_f53(unsigned c) : byte_count(c);

_Array_ptr<int> f54(unsigned num) {
  _Array_ptr<int> p : byte_count(num) = test_f53(num);
  return p;
}

_Array_ptr<int> f55(unsigned num1, unsigned num2){
  _Array_ptr<int> p : byte_count(num1) = test_f53(num2); // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}} \
                                                         // expected-note {{the declared upper bounds use the variable 'num1' and there is no relational information involving 'num1' and any of the expressions used by the inferred upper bounds}} \
                                                         // expected-note {{the inferred upper bounds use the variable 'num2' and there is no relational information involving 'num2' and any of the expressions used by the declared upper bounds}} \
                                                         // expected-note {{(expanded) declared bounds are 'bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + num1)'}} \
                                                         // expected-note {{inferred bounds are 'bounds((_Array_ptr<char>)value of test_f53(num2), (_Array_ptr<char>)value of test_f53(num2) + num2)'}}
  return p;
}

_Array_ptr<int> test_f70(int c) : byte_count(c);
_Nt_array_ptr<int> test_f70_n(int c) : byte_count(c);

_Array_ptr<int> f70(int num){
  _Array_ptr<int> p : byte_count(0) = test_f70(num); // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}} \
                                                     // expected-note {{the inferred upper bounds use the variable 'num' and there is no relational information involving 'num' and any of the expressions used by the declared upper bounds}} \
                                                     // expected-note {{declared bounds are 'bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + 0)'}} \
                                                     // expected-note {{inferred bounds are 'bounds((_Array_ptr<char>)value of test_f70(num), (_Array_ptr<char>)value of test_f70(num) + num)'}}
  return p;
}

_Nt_array_ptr<int> f70_n(int num){
  _Nt_array_ptr<int> p : byte_count(0) = test_f70_n(num); // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after initialization}} \
                                                          // expected-note {{the inferred upper bounds use the variable 'num' and there is no relational information involving 'num' and any of the expressions used by the declared upper bounds}} \
                                                          // expected-note {{(expanded) declared bounds are 'bounds((_Array_ptr<char>)p, (_Array_ptr<char>)p + 0)'}} \
                                                          // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)value of test_f70_n(num), (_Array_ptr<char>)value of test_f70_n(num) + num)'}}
  return p;
}

_Ptr<int> test_f56(unsigned c);

_Array_ptr<int> f57(unsigned num) {
  _Array_ptr<int> p : count(1) = test_f56(num);
  return p;
}

_Nt_array_ptr<char> f57_n(unsigned num) {
  _Nt_array_ptr<char> p : count(1) = test_f56(num); // expected-error {{initializing '_Nt_array_ptr<char>' with an expression of incompatible type '_Ptr<int>'}}
  return p;
}

_Array_ptr<int> f58(unsigned num) {
  _Array_ptr<int> p : count(2) = test_f56(num);    // expected-error {{declared bounds for 'p' are invalid after initialization}} \
                                                   // expected-note {{destination bounds are wider than the source bounds}} \
                                                   // expected-note {{destination upper bound is above source upper bound}} \
                                                   // expected-note {{(expanded) declared bounds are 'bounds(p, p + 2)'}} \
                                                   // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<int>)value of test_f56(num), (_Array_ptr<int>)value of test_f56(num) + 1)'}}
  return p;
}

_Array_ptr<int> f59(unsigned num) {
  _Array_ptr<int> p : count(0) = test_f56(num);
  return p;
}

_Nt_array_ptr<int> f60(unsigned num) {
  _Nt_array_ptr<int> p : count(0) = test_f56(num); // expected-error {{initializing '_Nt_array_ptr<int>' with an expression of incompatible type '_Ptr<int>'}}
  return p;
}

//

// Test pointer artihmetic equivalence

//

_Itype_for_any(T) void *simulate_calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
_Itype_for_any(T) void *simulate_malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

#pragma CHECKED_SCOPE OFF

struct s1 {
  _Ptr<struct s1> next;
};

#pragma CHECKED_SCOPE ON

struct s2 {
  int s2a;
  int s2b;
};

void a_f_1(int num1, int num2) {
  short n = num1/num2;
  _Array_ptr<long> v : count(n) = 0; // expected-note 2 {{(expanded) declared bounds are 'bounds(v, v + n)'}}
  _Array_ptr<int> v2 : count(n) = 0;
  v = simulate_calloc<long>(n, sizeof(long));           // expected-warning {{cannot prove declared bounds for 'v' are valid after assignment}} \
                                                        // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)value of simulate_calloc(n, sizeof(long)), (_Array_ptr<char>)value of simulate_calloc(n, sizeof(long)) + (size_t)n * sizeof(long))'}}
  v = simulate_calloc<long>(n, sizeof(unsigned long));  // expected-warning {{cannot prove declared bounds for 'v' are valid after assignment}} \
                                                        // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)value of simulate_calloc(n, sizeof(unsigned long)), (_Array_ptr<char>)value of simulate_calloc(n, sizeof(unsigned long)) + (size_t)n * sizeof(unsigned long))'}}
}

void a_f_1_size_t(int num1, int num2) {
  size_t n = num1/num2;
  _Array_ptr<long> v : count(n) = 0;
  _Array_ptr<int> v2 : count(n) = 0;
  v = simulate_calloc<long>(n, sizeof(long));
  v = simulate_calloc<long>(n, sizeof(unsigned long));
}

static size_t k = 0;

extern _Array_ptr<long> v2 : count(k + 1);
void a_f_2(void) {
  v2 = simulate_malloc<long>((k + 1) * sizeof(long));
  v2 = simulate_malloc<long>(sizeof(long) * (k + 1));
}

extern _Array_ptr<long> v10 : count(k + 1); // expected-note {{(expanded) declared bounds are 'bounds(v10, v10 + k + 1)'}}
void a_f_3(void) {
  v10 = simulate_malloc<long>((k + 1) * -1); // expected-warning {{cannot prove declared bounds for 'v10' are valid after assignment}} \
                                             // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)value of simulate_malloc((k + 1) * -1), (_Array_ptr<char>)value of simulate_malloc((k + 1) * -1) + (k + 1) * -1)'}}
}

extern _Array_ptr<long> v11 : count(k); // expected-note {{(expanded) declared bounds are 'bounds(v11, v11 + k)'}}
void a_f_4(void) {
  v11 = simulate_malloc<long>(k * 1); // expected-warning {{cannot prove declared bounds for 'v11' are valid after assignment}} \
                                      // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)value of simulate_malloc(k * 1), (_Array_ptr<char>)value of simulate_malloc(k * 1) + k * 1)'}}
}

extern _Array_ptr<long> v12 : count(k); // expected-note {{(expanded) declared bounds are 'bounds(v12, v12 + k)'}}
void a_f_5(void) {
  v12 = simulate_malloc<long>(k); // expected-warning {{cannot prove declared bounds for 'v12' are valid after assignment}} \
                                  // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)value of simulate_malloc(k), (_Array_ptr<char>)value of simulate_malloc(k) + k)'}}
}

extern _Array_ptr<long> v25_l : count(k);
void a_f_6_l(void) {
  v25_l = simulate_malloc<long>(k * __SIZEOF_LONG__);
}

extern _Array_ptr<long> v25_r : count(k);
void a_f_6_r(void) {
  v25_r = simulate_malloc<long>(__SIZEOF_LONG__ * k);
}

extern _Array_ptr<struct s2> v3 : count(k);
void a_f_7(void) {
  v3 = simulate_malloc<struct s2>(k * sizeof(struct s2));
}

extern _Array_ptr<struct s2> v32 : count(k);
void a_f_8(void) {
  v32 = simulate_malloc<struct s2>(sizeof(struct s2) * k);
}

extern _Array_ptr<struct s2> v4 : count(k); // expected-note {{(expanded) declared bounds are 'bounds(v4, v4 + k)'}}
void a_f_9(void) {
  v4 = simulate_malloc<struct s2>(k * sizeof(int)); // expected-warning {{cannot prove declared bounds for 'v4' are valid after assignment}} \
                                                    // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)value of simulate_malloc(k * sizeof(int)), (_Array_ptr<char>)value of simulate_malloc(k * sizeof(int)) + k * sizeof(int))'}}
}

extern _Array_ptr<int> v20 : count(k); // expected-note {{(expanded) declared bounds are 'bounds(v20, v20 + k)'}}
void a_f_10(void) {
  v20 = simulate_malloc<int>(k * sizeof(unsigned long long)); // expected-warning {{cannot prove declared bounds for 'v20' are valid after assignment}} \
                                                              // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)value of simulate_malloc(k * sizeof(unsigned long long)), (_Array_ptr<char>)value of simulate_malloc(k * sizeof(unsigned long long)) + k * sizeof(unsigned long long))'}}
}

typedef _Ptr<struct s1> t1;
struct s3 {
  _Array_ptr<t1> array : count(size);
  int size;
};

struct s4 {
  _Ptr<struct s4> next;
};

typedef _Ptr<struct s4> t2;
int v33;
t2 v = 0, v22 = 0;
_Array_ptr<struct s4> v24 : count(v33) = 0; // expected-note {{(expanded) declared bounds are 'bounds(v24, v24 + v33)'}}
void a_f_11(void) {
  v24 = simulate_calloc<struct s4>(v33, sizeof(*v22)); // expected-warning {{cannot prove declared bounds for 'v24' are valid after assignment}} \
                                                       // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)value of simulate_calloc(v33, sizeof (*v22)), (_Array_ptr<char>)value of simulate_calloc(v33, sizeof (*v22)) + (size_t)v33 * sizeof (*v22))'}}
}

size_t v34;
_Array_ptr<struct s4> v25 : count(v34) = 0;
void a_f_11_u(void) {
  v25 = simulate_calloc<struct s4>(v34, sizeof(*v22));
}

static _Array_ptr<char> x1 : count(k); // expected-note {{(expanded) declared bounds are 'bounds(x1, x1 + k)'}}
static _Array_ptr<char> x2 : count(3);
void a_f_12(void) {
  x1 = simulate_calloc<char>(32768, sizeof(char)); // expected-error {{it is not possible to prove that the inferred bounds of 'x1' imply the declared bounds of 'x1' after assignment}} \
                                                   // expected-note {{the declared upper bounds use the variable 'k' and there is no relational information involving 'k' and any of the expressions used by the inferred upper bounds}} \
                                                   // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)value of simulate_calloc(32768, sizeof(char)), (_Array_ptr<char>)value of simulate_calloc(32768, sizeof(char)) + (size_t)32768 * sizeof(char))'}}
  x2 = simulate_calloc<char>(3, sizeof(char));
}

typedef struct ts1 {
  _Ptr<struct ts1> next;
} ts1;

void a_f_13(int n) {
  _Array_ptr<_Ptr<ts1>> v22 : count(n) = simulate_calloc<_Ptr<ts1>>(n, (sizeof(_Ptr<ts1>))); // expected-warning {{cannot prove declared bounds for 'v22' are valid after initialization}} \
                                                                                             // expected-note {{(expanded) declared bounds are 'bounds(v22, v22 + n)'}} \
                                                                                             // expected-note {{(expanded) inferred bounds are 'bounds((_Array_ptr<char>)value of simulate_calloc(n, (sizeof(_Ptr<ts1>))), (_Array_ptr<char>)value of simulate_calloc(n, (sizeof(_Ptr<ts1>))) + (size_t)n * (sizeof(_Ptr<ts1>)))'}}
}

void a_f_13_u(size_t n) {
  _Array_ptr<_Ptr<ts1>> v22 : count(n) = simulate_calloc<_Ptr<ts1>>(n, (sizeof(_Ptr<ts1>)));
}

void a_f_14(void) {
  long i;
  _Array_ptr<long> v21 : count(i + 1) = simulate_malloc<long>((i + 1) * (i + 1) * sizeof(long)); // expected-warning {{cannot prove declared bounds for 'v21' are valid after initialization}} \
                                                                                                 // expected-note {{(expanded) declared bounds are 'bounds(v21, v21 + i + 1)'}} \
                                                                                                 // expected-note {{(expanded) inferred bounds}}
}

void a_f_15(void) {
  long i, j;
  _Array_ptr<long> v : count(j + 1) = simulate_malloc<long>((i + 1) * sizeof(long)); // expected-error {{it is not possible to prove that the inferred bounds of 'v' imply the declared bounds of 'v' after initialization}} \
                                                                                     // expected-note {{the declared upper bounds use the variable 'j' and there is no relational information involving 'j' and any of the expressions used by the inferred upper bounds}} \
                                                                                     // expected-note {{the inferred upper bounds use the variable 'i' and there is no relational information involving 'i' and any of the expressions used by the declared upper bounds}} \
                                                                                     // expected-note {{(expanded) declared bounds are 'bounds(v, v + j + 1)'}} \
                                                                                     // expected-note {{(expanded) inferred bounds}}
}

static _Array_ptr<char> v23 : count(32768);
static _Array_ptr<void> a_f_16(int size) : byte_count(size) {
  v23 = simulate_calloc<char>(32768, sizeof(char));
  return v23;
}

//
// Test use of the extracted comparison facts for proving bounds declarations
//

_Itype_for_any(T) void *simulate_memcpy(void * restrict dest : itype(restrict _Array_ptr<T>) byte_count(n),
             const void * restrict src : itype(restrict _Array_ptr<const T>) byte_count(n),
             size_t n) : itype(_Array_ptr<T>) byte_count(n);

_Array_ptr<int> f80(unsigned num1, unsigned num2) {
  if (num1 > num2)
    return 0;
  _Array_ptr<int> p : count(num1) = test_f50(num2);
  return p;
}

void f81(_Ptr<int> d, size_t a) {
  _Array_ptr<_Ptr<int>> b : count(a) = 0;
  _Array_ptr<_Ptr<int>> c : count(a) = 0;
  if (a == 0 || a < *d)
    return;
  b = simulate_malloc<_Ptr<int>>(a * sizeof(_Ptr<int>));
  if (b == 0)
    return;
  if (c != 0 && *d > 0)
    simulate_memcpy<_Ptr<int>>(b, c, *d * sizeof(_Ptr<int>));
}

void f82(_Ptr<int> d, size_t a) {
  _Array_ptr<_Ptr<int>> b : count(a) = 0;
  _Array_ptr<_Ptr<int>> c : count(a) = 0;
  if (a == 0)
    return;
  b = simulate_malloc<_Ptr<int>>(a * sizeof(_Ptr<int>));
  if (b == 0)
    return;
  if (c != 0 && *d > 0)
    simulate_memcpy<_Ptr<int>>(b, c, *d * sizeof(_Ptr<int>)); // expected-warning {{cannot prove argument meets declared bounds for 1st parameter}} \
                                                              // expected-note {{expected argument bounds are 'bounds((_Array_ptr<char>)b, (_Array_ptr<char>)b + *d * sizeof(_Ptr<int>))'}} \
                                                              // expected-note {{inferred bounds are 'bounds(b, b + a)'}} \
                                                              // expected-warning {{cannot prove argument meets declared bounds for 2nd parameter}} \
                                                              // expected-note {{expected argument bounds are 'bounds((_Array_ptr<char>)(_Array_ptr<_Ptr<int> const>)c, (_Array_ptr<char>)(_Array_ptr<_Ptr<int> const>)c + *d * sizeof(_Ptr<int>))'}} \
                                                              // expected-note {{inferred bounds are 'bounds(c, c + a)'}}
}

void f83(_Ptr<int> d, size_t a) {
  _Array_ptr<_Ptr<int>> b : count(a) = 0;
  _Array_ptr<_Ptr<int>> c : count(a) = 0;
  if (a == 0 || a < *d)
    return;
  b = simulate_malloc<_Ptr<int>>(a * sizeof(_Ptr<int>));
  if (b == 0)
    return;
  if (c != 0 && *d > 0)
    simulate_memcpy<_Ptr<int>>(b, c, *d * sizeof(_Ptr<int>));
}

struct st_80;
struct st_80_arr {
  struct st_80 **e : itype(_Array_ptr<_Ptr<struct st_80>>) count(c);
  int d;
  int c;
};

void f84(_Ptr<struct st_80_arr> arr, int b) {
  _Array_ptr<_Ptr<struct st_80>> a : count(b) = 0;
  if (arr->c <= b) {
    arr->c = b * b;
    arr->e = a; // expected-warning {{cannot prove declared bounds for arr->e are valid after assignment}} \
                // expected-note {{declared bounds are 'bounds(arr->e, arr->e + arr->c)'}} \
                // expected-note {{inferred bounds are 'bounds(a, a + b)'}}
  }
}

void f85(_Ptr<struct st_80_arr> arr, int b) {
  _Array_ptr<_Ptr<struct st_80>> a : count(b) = 0;
  if (arr->c <= b) {
    arr->e = a; // expected-warning {{cannot prove declared bounds for arr->e are valid after assignment}} \
                // expected-note {{declared bounds are 'bounds(arr->e, arr->e + arr->c)'}} \
                // expected-note {{inferred bounds are 'bounds(a, a + b)'}}
    arr->c = b * b;
  }
}
