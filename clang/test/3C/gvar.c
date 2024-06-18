// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/gvar.c -- | diff %t.checked/gvar.c -

int *x;
//CHECK: _Ptr<int> x = ((void *)0);
extern int *x;
void foo(void) {
  //CHECK: void foo(void) _Checked {
  *x = 1;
}

extern int *y;
int *y;
//CHECK: int *y;
int *bar(void) {
  //CHECK: _Ptr<int> bar(void) {
  y = (int *)5;
  //CHECK: y = (int *)5;
  return x;
}
