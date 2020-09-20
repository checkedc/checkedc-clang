// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

void foo(void) { 
  //CHECK: void foo(void) {
  int *b = (int*) 1;
  //CHECK: int *b = (int*) 1;
  if (1) { 
    //CHECK: if (1) {
    b;
    while(1) { 
      //CHECK: while(1) {
      b;
      int x =3;
    }
  }

}


// Dummy function
void dummy(int *x) { 
  //CHECK: void dummy(_Ptr<int> x) _Checked {
}
