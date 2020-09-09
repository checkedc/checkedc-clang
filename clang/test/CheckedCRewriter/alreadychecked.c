// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

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

