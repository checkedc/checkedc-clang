// RUN: 3c -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked -alltypes %s
// RUN: 3c -alltypes %S/alreadychecked.checked.c -- | count 0
// RUN: rm %S/alreadychecked.checked.c

int fib(int n) _Checked
//CHECK: int fib(int n) _Checked
{
  if (n == 0) { return 0; }
  //CHECK: if (n == 0) { return 0; }
  if (n == 1) { return 1; }
  //CHECK: if (n == 1) { return 1; }
  return fib(n-1) + fib(n-2);
}


int *x;
//CHECK: _Ptr<int> x = ((void *)0);

