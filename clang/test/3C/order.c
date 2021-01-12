// RUN: 3c %s | FileCheck -match-full-lines %s
// RUN: 3c %s | %clang -c -fcheckedc-extension -x c -o %t.unused -

int foo1(int x, int *y);
//CHECK: int foo1(int x, _Ptr<int> y);

int foo1();
//CHECK: int foo1(int x, _Ptr<int> y);

int bar1(void) { 
  int *z = (int*) 4;
  return foo1(4, z);
}

int foo1(int x, int *y) { 
//CHECK: int foo1(int x, _Ptr<int> y) { 
  return x + *y;
}


int foo2(int x, int *y);
//CHECK: int foo2(int x, _Ptr<int> y);

int bar2(void) { 
  int *z = (int*) 4;
  return foo2(4, z);
}

int foo2();
//CHECK: int foo2(int x, _Ptr<int> y);

int foo2(int x, int *y) { 
//CHECK: int foo2(int x, _Ptr<int> y) { 
  return x + *y;
}


int foo3(int x, int *y) { 
//CHECK: int foo3(int x, _Ptr<int> y) { 
  return x + *y;
}

int bar3(void) { 
  int *z = (int*) 4;
  return foo3(4, z);
}

int foo3(int x, int *y);
//CHECK: int foo3(int x, _Ptr<int> y);

int foo3();
//CHECK: int foo3(int x, _Ptr<int> y);

