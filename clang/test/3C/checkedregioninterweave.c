// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -


void foo(int *x) { 
  //CHECK: void foo(_Ptr<int> x) _Checked {
  while(1) { 
    //CHECK: while(1) {
    *x += 1;
    if(0) { 
      //CHECK: if(0) _Unchecked {
      int *y = (int*) 3;
      //CHECK: int *y = (int*) 3;
      if(0) { 
        //CHECK: if(0) {
        *y += 1;
        while(0) { 
          //CHECK: while(0) _Checked {
          *x += 1;
        }
      }
    }
  }
    

}
