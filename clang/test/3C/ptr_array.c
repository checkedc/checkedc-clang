// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/ptr_array.c -- | diff %t.checked/ptr_array.c -

/* Tests for issue 60. Array initialization had not been implemented, so wild
 pointer inside and array initializer did not cause the array to be an array
 of wild pointer. */

/* A checked pointer should allow the array to be checked. */
void test0(int *a) {
  //CHECK_NOALL: void test0(int *a : itype(_Ptr<int>)) {
  //CHECK_ALL: void test0(_Ptr<int> a) _Checked {

  int *b[1] = {a};
  //CHECK_NOALL: int *b[1] = {a};
  //CHECK_ALL: _Ptr<int> b _Checked[1] = {a};
}

/* An unchecked pointer should cause the array to be unchecked. */
void test1(int *a) {
  //CHECK: void test1(int *a : itype(_Ptr<int>)) {

  a = (int *)4;
  //CHECK: a = (int *)4;

  int *b[1] = {a};
  //CHECK_NOALL: int *b[1] = {a};
  //CHECK_ALL: int *b _Checked[1] = {a};
}

/* Example from from the issue */
int *foo() {
  //CHECK: int *foo(void) : itype(_Ptr<int>) {

  int x = 1;
  int y = 2;
  int z = 3;
  int *ptrs[4] = {&x, &y, &z, (int *)5};
  //CHECK_NOALL: int *ptrs[4] = {&x, &y, &z, (int *)5};
  //CHECK_ALL: int *ptrs _Checked[4] = {&x, &y, &z, (int *)5};
  int *ret;
  //CHECK: int *ret;
  for (int i = 0; i < 4; i++) {
    ret = ptrs[i];
  }

  return ret;
}

/* Example from the issue, but everthing should check */
int *foo2() {
  //CHECK_NOALL: int *foo2(void) : itype(_Ptr<int>) {
  //CHECK_ALL: _Ptr<int> foo2(void) _Checked {

  int x = 1;
  int y = 2;
  int z = 3;
  int *ptrs[4] = {&x, &y, &z, &x};
  //CHECK_NOALL: int *ptrs[4] = {&x, &y, &z, &x};
  //CHECK_ALL: _Ptr<int> ptrs _Checked[4] = {&x, &y, &z, &x};
  int *ret;
  //CHECK_NOALL: int *ret;
  //CHECK_ALL: _Ptr<int> ret = ((void *)0);
  for (int i = 0; i < 4; i++) {
    ret = ptrs[i];
  }

  return ret;
}
