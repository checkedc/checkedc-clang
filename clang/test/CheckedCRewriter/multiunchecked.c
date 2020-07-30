// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s


int foo(void) { 
//CHECK: int foo(void) _Checked { 
  if(1) { 
    //CHECK: if(1) _Unchecked {
    int *x = (int*) 3;
    //CHECK: int *x = (int*) 3;
    return *x;
  } 
  if(1) { 
    //CHECK: if(1) _Unchecked {
    int *x = (int*) 3;
    //CHECK: int *x = (int*) 3;
    return *x;
  }
}

