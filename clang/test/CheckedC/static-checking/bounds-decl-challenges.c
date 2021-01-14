// Tests that the current static checking of bounds declaration can't
// handle.
//
// RUN: %clang -cc1 -fcheckedc-extension -Wcheck-bounds-decls -verify -verify-ignore-unexpected=note %s

// Deferencing an array of nt_checked arrays.
extern void check_assign(int val, int p[10], int q[], int r _Checked[10], int s _Checked[],
                         int s2d _Checked[10][10], int v _Nt_checked[10], int w _Nt_checked[],
                         int w2d _Checked[10]_Nt_checked[10]) {
  int x2d _Checked[10]_Nt_checked[10];
  _Nt_array_ptr<int> t13b = w2d[0];  // expected-warning {{cannot prove declared bounds for 't13b' are valid after initialization}}
  _Nt_array_ptr<int> t15b = x2d[0];  // expected-warning {{cannot prove declared bounds for 't15b' are valid after initialization}}
}

// Creating a pointer with count bounds into an existing array.
void passing_test_1(void) {
  int a _Checked[10] = { 9, 8, 7, 6, 5, 4, 3, 2, 1};
  _Array_ptr<int> b : count(5) = &a[2];  // expected-warning {{cannot prove declared bounds for 'b' are valid after initialization}}
}
