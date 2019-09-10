// Test static checking thrat identifies out-of-bounds accesses.  Currently
// we use information from bounds that are constant-sized ranges.  This
// is turned on by default because we only warn when an out-of-bounds access
// is found.
//
// RUN: %clang -cc1 -fcheckedc-extension -verify -verify-ignore-unexpected=note %s

void f1(int arr _Checked[10]) {
  int i = arr[-1]; // expected-warning {{out-of-bounds memory access}}
  i = arr[0];
  i = arr[10];     // expected-warning {{out-of-bounds memory access}}
  i = arr[11];     // expected-warning {{out-of-bounds memory access}}
  arr[0] = 1;
  arr[-1] = 0;     // expected-warning {{out-of-bounds memory access}}
  arr[10] = 0;     // expected-warning {{out-of-bounds memory access}}
  arr[11] = 0;     // expected-warning {{out-of-bounds memory access}}
}

void f2(_Array_ptr<int> p : count(10)) {
  int i = p[-1];  // expected-warning {{out-of-bounds memory access}}
  i = p[0];
  i = p[10];      // expected-warning {{out-of-bounds memory access}}
  i = p[11];      // expected-warning {{out-of-bounds memory access}}
  i = *(p + 0);
  i = *(p - 1);   // expected-warning {{out-of-bounds memory access}}
  i = *(p + 10);  // expected-warning {{out-of-bounds memory access}}
  i = *(p + 11);  // expected-warning {{out-of-bounds memory access}}

  p[-1] = i;      // expected-warning {{out-of-bounds memory access}}
  p[0] = i;
  p[10] = i;      // expected-warning {{out-of-bounds memory access}}
  p[11] = i;      // expected-warning {{out-of-bounds memory access}}
  *(p + 0) = i;
  *(p - 1) = i ;   // expected-warning {{out-of-bounds memory access}}
  *(p + 10) = i;  // expected-warning {{out-of-bounds memory access}}
  *(p + 11) = i;  // expected-warning {{out-of-bounds memory access}}
}

void f3(_Nt_array_ptr<int> p) {
  int i = *p;  
  *p = i;        // expected-warning {{out-of-bounds memory access}}
  i = *(p + 1);  // expected-warning {{out-of-bounds memory access}}
  *(p + 1) = i;  // expected-warning {{out-of-bounds memory access}}
}

int global_arr1 _Checked[10];
void f4(void) {
  int i = global_arr1[0];
  i = global_arr1[-1];     // expected-warning {{out-of-bounds memory access}} \
                           // expected-warning {{array index -1 is before the beginning of the array}}
  i = global_arr1[11];     // expected-warning {{out-of-bounds memory access}} \
                           // expected-warning {{array index 11 is past the end of the array (which contains 10 elements)}}                            
  i = *(global_arr1 - 1);  // expected-warning {{out-of-bounds memory access}} 
  i = *(global_arr1 + 11); // expected-warning {{out-of-bounds memory access}} 
  i = *(global_arr1 + 0);
  global_arr1[-1] = i;     // expected-warning {{out-of-bounds memory access}} \
                           // expected-warning {{array index -1 is before the beginning of the array}}
  global_arr1[11] = i;     // expected-warning {{out-of-bounds memory access}} \
                           // expected-warning {{array index 11 is past the end of the array (which contains 10 elements)}}                            
  *(global_arr1 - 1) = i;  // expected-warning {{out-of-bounds memory access}} 
  *(global_arr1 + 11) = i; // expected-warning {{out-of-bounds memory access}} 
  *(global_arr1 + 0) = i;
}

struct S { int i;  int j; };
struct S global_arr2 _Checked[10];
void f5(_Array_ptr<struct S> param_arr : count(10)) {
  _Ptr<int> p = &(global_arr2[11].i); // expected-warning {{out-of-bounds memory access}} expected-warning {{array index 11 is past the end of the array}}
                                      // TODO: generate better error message for this situation
  p = &((global_arr2 + 11)->i); // expected-warning {{out-of-bounds base value}}
  p = &(param_arr[11].i); // expected-warning {{out-of-bounds memory access}}  
                          // TODO: generate better error message for this situation
  p = &((param_arr + 11)->i);   // expected-warning {{out-of-bounds base value}}

  int i = global_arr2[11].i; // expected-warning {{out-of-bounds memory access}} expected-warning {{array index 11 is past the end of the array}}
  i = (global_arr2 + 11)->i; // expected-warning {{out-of-bounds base value}}
  i = param_arr[11].i;       // expected-warning {{out-of-bounds memory access}}  
  i = (param_arr + 11)->i;   // expected-warning {{out-of-bounds base value}}
}

void f6(_Array_ptr<int> p : count(0)) {
  int i = *p;  // expected-warning {{out-of-bounds memory access}}  
  *p = i;  // expected-warning {{out-of-bounds memory access}}  
}
