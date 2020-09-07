// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s

int foo();
//CHECK: int foo(_Array_ptr<int> x : count(y), int y);

int bar(int *x, int c) { 
//CHECK: int bar(_Array_ptr<int> x : count(c), int c) { 
  return foo(x, 1) + 3;
}

int foo(int *x, int y);
//CHECK: int foo(_Array_ptr<int> x : count(y), int y);

int foo(int *x, int y) { 
//CHECK: int foo(_Array_ptr<int> x : count(y), int y) { 
  int sum = 0;
  for(int i = 0; i < y; i++) { 
    sum += x[i];
  }
  return sum;
}

int foo2(int *x, int y);
//CHECK: int foo2(_Array_ptr<int> x : count(y), int y);

int bar2(int *x, int c) { 
//CHECK: int bar2(_Array_ptr<int> x : count(c), int c) { 
  return foo(x, c) + 3;
}

int foo2();
//CHECK: int foo2(_Array_ptr<int> x : count(y), int y);

int foo2(int *x, int y) { 
//CHECK: int foo2(_Array_ptr<int> x : count(y), int y) { 
  int sum = 0;
  for(int i = 0; i < y; i++) { 
    sum += x[i];
  }
  return sum;
}

void f(int *x) { *x = 1; }
//CHECK: void f(_Ptr<int> x) { *x = 1; }

void f(int *x);
//CHECK: void f(_Ptr<int> x);

void call(void) {
  int *x = 0;
  //CHECK: _Ptr<int> x = 0;
  f(x);
}

