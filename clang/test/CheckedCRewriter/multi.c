// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s

char* bad(void) { 
  //CHECK: char* bad(void) {
  return (char*) 3;
}


void f(void) { 
  //CHECK: void f(void) _Checked {
  int x = 3;
  if(x) { 
    //CHECK: if(x) _Unchecked {
    bad();
  } else { 
    //CHECK: } else _Unchecked {
    bad();
  }
}
