// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/fp.c -- | diff %t.checked/fp.c -

int f(int *p);
//CHECK: int f(int *p);
void bar() {
  int (*fp)(int *p) = f;
  //CHECK: _Ptr<int (int *p)> fp = f;
  f((void *)0);
}

int mul_by_2(int x) {
  //CHECK: int mul_by_2(int x) _Checked {
  return x * 2;
}

int (*foo(void))(int) {
  //CHECK: _Ptr<int (int)> foo(void) _Checked {
  return mul_by_2;
}
