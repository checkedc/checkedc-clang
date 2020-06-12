// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s

void foo(int *x) {
  struct bar { int *x; } *y = 0;
} 
//CHECK: struct bar { _Ptr<int> x; } *y = 0;

void baz(int *x) {
  struct bar { char *x; } *w = 0;
} 
//CHECK: struct bar { _Ptr<char> x; } *w = 0;

