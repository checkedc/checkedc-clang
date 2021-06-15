// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/alreadychecked.c -- | diff %t.checked/alreadychecked.c -

int fib(int n) _Checked
//CHECK: int fib(int n) _Checked
{
  if (n == 0) {
    return 0;
  }
  //CHECK: if (n == 0) {
  if (n == 1) {
    return 1;
  }
  //CHECK: if (n == 1) {
  return fib(n - 1) + fib(n - 2);
}

int *x;
//CHECK: _Ptr<int> x = ((void *)0);
