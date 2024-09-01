// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/subtyp.c -- | diff %t.checked/subtyp.c -

void take(int *p : itype(_Nt_array_ptr<int>));
//CHECK: void take(int *p : itype(_Nt_array_ptr<int>));
int *foo(int *x) {
  //CHECK_NOALL: int *foo(int *x : itype(_Ptr<int>)) : itype(_Ptr<int>) {
  //CHECK_ALL: _Nt_array_ptr<int> foo(_Nt_array_ptr<int> x) _Checked {
  take(x);
  return x;
}
void bar() {
  //CHECK: void bar() _Checked {
  int *x = 0;
  //CHECK_NOALL: _Ptr<int> x = 0;
  //CHECK_ALL: _Nt_array_ptr<int> x = 0;
  foo(x);
}
void baz() {
  int *x = (int *)5;
  //CHECK: int *x = (int *)5;
  foo(x);
  //CHECK_NOALL: foo(x);
  //CHECK_ALL: foo(_Assume_bounds_cast<_Nt_array_ptr<int>>(x, bounds(unknown)));
}
void force(int *x) {}
//CHECK: void force(_Ptr<int> x) _Checked {}
