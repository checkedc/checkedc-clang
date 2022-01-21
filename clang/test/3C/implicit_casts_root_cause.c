// RUN: 3c -base-dir=%S -alltypes -warn-root-cause %s -- -Xclang -verify -Wno-everything

// This test checks that void* casts of arguments are ignored by the root
// cause code (and handled by the function parameter causes), but other casts
// are still noted.

void has_void(void* v); // expected-warning {{3 unchecked pointers: Default void* type}}
void test_no_cause() {
  int *b, *c;
  has_void(b); // specifically no warning here, since it shows up above
  has_void(c);
}

void has_float(float* v); // expected-warning {{Unchecked pointer in parameter or return of undefined function has_float}}
void test_float_cause() {
  int *b, *c;
  has_float(b); // expected-warning {{1 unchecked pointer: Cast from int * to float *}}
  has_float(c); // expected-warning {{1 unchecked pointer: Cast from int * to float *}}
}

void has_body(float* v){}
void test_only_args_cause() {
  int *b, *c;
  has_body(b); // expected-warning {{1 unchecked pointer: Cast from int * to float *}}
  has_body(c); // expected-warning {{1 unchecked pointer: Cast from int * to float *}}
}
