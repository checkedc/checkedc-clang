// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s

int f(void) { 
  //CHECK: int f(void) {
  char* u = (char*) 3;
  //CHECK: char* u = (char*) 3;

  if(1) { 
    //CHECK: if(1) _Checked {
    return 1;
  } else { 
    //CHECK: } else _Checked {
    return 2;
  }
}
