// Tests for checking that bounds declarations hold after assignments to
// variables or members used in a bounds declaration, but not the subject of
// the bounds declaration.

// The implementation is not complete enough to print diagnostics, so we don't
//have any yet.  Add comments where diagnostics will be expected.
//
// RUN: %clang_cc1 -Wcheck-bounds-decls -verify -verify-ignore-unexpected=warning -verify-ignore-unexpected=note %s

//
// Test bounds declarations involving global variables.
//

// Test different forms of bounds expressions
extern _Array_ptr<int> alloc(int i) : count(i);

int g1_len;
_Array_ptr<int> g2 : count(g1_len);

int g3_len;
_Array_ptr<int> g4 : byte_count(g3_len);

_Array_ptr<int> g3_low;
_Array_ptr<int> g4_high;
_Array_ptr<int> g5 : bounds(g3_low, g4_high);
int g6_arr[10];

void f1(int i) {
  g1_len = i, g2 = alloc(i * sizeof(int));  // correct
  g1_len = 5;                               // incorrect

  g3_len = i * sizeof(int), g4 = alloc(i * sizeof(int)); // correct
  g3_len = 10;                                           // incorrect
  g3_low = g6_arr + 2, g4_high = g6_arr + 6, g5 = g6_arr + 2;  // correct
  g3_low = g2;                                                 // incorrect.

  {
     // Declare a bounds declaration that goes out of scope;
     g1_len = i;
     _Array_ptr<int> x1 : count(g1_len) = alloc(i * sizeof(int));
     g1_len = 5; // expected-error {{it is not possible to prove that the inferred bounds of 'x1' imply the declared bounds of 'x1' after assignment}}
  }
}

// Test multiple variables with bounds dependent upon one variable.
int g6_len;
_Array_ptr<int> g7 : count(g6_len);
_Array_ptr<int> g8 : count(g6_len);
void f2(int i) {
  g6_len = i, g7 = alloc(i * sizeof(int)), g8 = alloc(i * sizeof(int));  // correct
  g6_len = 5;   // incorrect.
}

// Test bounds declarations that are only dependent on the variable
// being declared.  We want to make sure these are not processed twice.
int g9_len;
_Array_ptr<int> g10 : bounds(g10, g10 + 5);
_Array_ptr<int> g11 : bounds(g11, g11 + g9_len);

void f3(int i) {
  g10 = alloc(5 * sizeof(int));
  g9_len = i, g11 = alloc(i * sizeof(int));   // correct
  g9_len = 5;                                 // incorrect

}

// Test array variables with declared bounds.
int g10_len;
int arr _Checked[10] : count(g10_len);

void f4(int i) {
  g10_len = 5;
}

// Test hiding a global variable with bounds, and
// hiding the variable used in bounds.
int g11_len;
_Array_ptr<int> g12 : count(g11_len);

void f5(int i) {
  int mylen = 0;
  _Array_ptr<int> g12 : count(mylen) = 0;
  g11_len = 5;       // incorrect
  int g11_len = 10;
}

//
// Test bounds declarations involving parameters.
//

// Test different forms of bounds declarations.
void f20(int len, _Array_ptr<int> p : count(len), int i) {
  len = i, p = alloc(i * sizeof(int));  // correct
  len = 5;                              // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after assignment}}
}

void f21(int len, _Array_ptr<int> p : byte_count(len), int i) {
  len = i * sizeof(int), p = alloc(i * sizeof(int)); // correct
  len = 10;                                          // expected-error {{inferred bounds for 'p' are unknown after assignment}}
}

void f22(_Array_ptr<int> p : bounds(low, high), _Array_ptr<int> low,
         _Array_ptr<int> high) {
  _Array_ptr<int> tmp : count(10) = alloc(10 * sizeof(int));
  p = tmp + 4, low = tmp, high = tmp + 10;  // correct
  low = g3_low;                             // incorrect
}

void f23(int len, _Array_ptr<int> p : count(len)) {
  {
     // Declare a bounds declaration that goes out of scope.
     _Array_ptr<int> t : count(len) = alloc(len * sizeof(int)); // correct
     len = 5; // expected-error {{inferred bounds for 'p' are unknown after assignment}} \
              // expected-error {{inferred bounds for 't' are unknown after assignment}}
  }
}

// Test hiding a parameter with bounds.
void f24(int len, _Array_ptr<int> p : count(len)) {
  // Create a new scope to avoid an error due to redefining a variable
  // in the same scope.  Parameters are in the same scope as top-level
  // locals.
  {
     int mylen = 0;
     _Array_ptr<int> p : count(mylen) = 0;
     len = 5;   // expected-error {{inferred bounds for 'p' are unknown after assignment}}
  }
}

// Test hiding a variable used in parameter bounds.
void f25(int len, _Array_ptr<int> p : count(len), int i) {

  {
     int len = i;  // correct.
  }
}


//
// Test bounds declarations involving local variables.
//

// Test different forms of bounds declarations.
void f40(int i) {
  int len = 0;
  _Array_ptr<int> p : count(len) = 0;
  len = i, p = alloc(i * sizeof(int));  // correct
  len = 5;                              // expected-error {{it is not possible to prove that the inferred bounds of 'p' imply the declared bounds of 'p' after assignment}}
}

void f41(int i) {
  int len = 0;
   _Array_ptr<int> p : byte_count(len) = 0;
  len = i * sizeof(int), p = alloc(i * sizeof(int)); // correct
  len = 10;                                          // expected-error {{inferred bounds for 'p' are unknown after assignment}}
}

void f42(void) {
  _Array_ptr<int> low = 0;
  _Array_ptr<int> high = 0;
  _Array_ptr<int> p : bounds(low, high) = 0;
  _Array_ptr<int> tmp : count(10) = alloc(10 * sizeof(int));
  p = tmp + 4, low = tmp, high = tmp + 10;  // correct
  low = g3_low;                             // incorrect
}

void f43(void)  {
  int len = 10;
  _Array_ptr<int> p : count(len) = alloc(len * sizeof(int));
  {
     // Declare a bounds declaration that goes out of scope.
     _Array_ptr<int> t : count(len) = alloc(len * sizeof(int)); // correct
     len = 5; // expected-error {{inferred bounds for 'p' are unknown after assignment}} \
              // expected-error {{inferred bounds for 't' are unknown after assignment}}
  }
}

// Test hiding a local variable with bounds.
void f44(void) {
  int len = 5;
 _Array_ptr<int> p : count(len) = alloc(sizeof(int) * len);
 {
    int mylen = 0;
    _Array_ptr<int> p : count(mylen) = 0;
     len = 5;   // expected-error {{inferred bounds for 'p' are unknown after assignment}}
  }
}

// Test hiding a variable used in parameter bounds.
void f45(int i) {
  int len = 5;
  _Array_ptr<int> p : count(len) = alloc((sizeof(int) * len));
  {
     int len = i;  // correct.
  }
}

// Spot check different forms of modifying expressions

void f100(int len, _Array_ptr<int> p : count(len), int i) {
  if (len > 1)
    len--; // expected-error {{inferred bounds for 'p' are unknown after decrement}}
}

void f101(int len, _Array_ptr<int> p : count(len), int i) {
  if (len > 1)
    --len; // expected-error {{inferred bounds for 'p' are unknown after decrement}}
}

void f102(int len, _Array_ptr<int> p : count(len), int i) {
  if (len > 1)
    len -= 1; // expected-error {{inferred bounds for 'p' are unknown after assignment}}
}

void f103(int len, _Array_ptr<int> p : count(len), int i) {
  if (len < 100)
    len++, p = alloc(len * sizeof(int));
}

void f104(int len, _Array_ptr<int> p : count(len), int i) {
  if (len < 100)
    ++len, p = alloc(len * sizeof(int));
}

void f105(int len, _Array_ptr<int> p : count(len), int i) {
  if (len < 100)
    len += 1, p = alloc(len * sizeof(int));
}

