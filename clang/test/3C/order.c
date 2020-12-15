// RUN: 3c %s -- | FileCheck -match-full-lines %s

int foo1(int x, int *y);
//CHECK: int foo1(int x, int *y : itype(_Ptr<int>));

int foo1();
//CHECK: int foo1(int x, int *y : itype(_Ptr<int>));

int bar1(void) { 
  int *z = (int*) 4;
  return foo1(4, z);
}

int foo1(int x, int *y) { 
//CHECK: int foo1(int x, int *y : itype(_Ptr<int>)) {
  return x + *y;
}


int foo2(int x, int *y);
//CHECK: int foo2(int x, int *y : itype(_Ptr<int>));

int bar2(void) { 
  int *z = (int*) 4;
  return foo2(4, z);
}

int foo2();
//CHECK: int foo2(int x, int *y : itype(_Ptr<int>));

int foo2(int x, int *y) { 
//CHECK: int foo2(int x, int *y : itype(_Ptr<int>)) {
  return x + *y;
}


int foo3(int x, int *y) { 
//CHECK: int foo3(int x, int *y : itype(_Ptr<int>)) {
  return x + *y;
}

int bar3(void) { 
  int *z = (int*) 4;
  return foo3(4, z);
}

int foo3(int x, int *y);
//CHECK: int foo3(int x, int *y : itype(_Ptr<int>));

int foo3();
//CHECK: int foo3(int x, int *y : itype(_Ptr<int>));

