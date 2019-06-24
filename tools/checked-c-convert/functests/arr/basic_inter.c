// here we test the propagation of constraints
// between functions.

// we test propagation with and without function
// declaration.
int funcdecl(int *ptr, int *iptr, int *arrwild);
int funcdecl(int *ptr, int *iptr, int *arrwild) {
   if(ptr != 0) {
    *ptr = 0;
   }   
   arrwild++;
}

// ptr is an arr ptr
// iptr will be itype
// wild will be a wild ptr.
int func(int *ptr, int *iptr, int *wild) {
   if(ptr != 0) {
      ptr[0] = 1;
   }
   wild = 0xdeadbeef;
}
int main() {
  int a, b, c;
  // this will be Ptr
  int *ap;
  // this will be WILD
  int *bp;
  // this will be _Ptr
  int *cp;
  // this will be _Ptr
  int *ap1;
  // this will be WILD
  int *bp1;
  // this will be _Ptr
  int *cp1;

  
  ap1 = &a;
  ap = &a;
  // we will make this pointer wild.
  bp1 = bp = 0xcafeba;
  cp = &c;
  cp1 = &c;
  // although, we are passing cp
  // to a paramter that will be 
  // treated as WILD in func, cp
  // is Ptr within main
  func(ap, bp, cp);
  funcdecl(ap1, bp1, cp1);
  

}
