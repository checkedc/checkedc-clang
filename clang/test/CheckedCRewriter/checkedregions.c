// RUN: cconv-standalone -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// Tests for adding (un)checked regions automatically

#define NULL 0

int foo(int *x) { 
  return *x;
}
//CHECK: int foo(_Ptr<int> x) _Checked {

int bar(int *x) { 
  int i;
  for(i = 0; i<2; i++) { 
    *x = i;
  }
  return *x;
}

//CHECK: int bar(_Ptr<int> x) _Checked { 
//CHECK: for(i = 0; i<2; i++) { 

int gar(int *x) { 
  x = (int*) 4;
  return *x;
}

//CHECK: int gar(int *x) {

