#include<string_checked.h>
// here we test the propagation of constraints
// between functions.

// we test propagation with and without function
// declaration.
// here, ntiptr will be an itype(Nt_ptr)
intfuncdecl(char *ntiptr : itype(_Nt_arr_ptr<char> ) , int *iptr : itype(_Ptr<int> ) , int *wild);
intfuncdecl(char *ntiptr : itype(_Nt_arr_ptr<char> ) , int *iptr : itype(_Ptr<int> ) , int *wild) {
   if(ntiptr != 0) {
    ntiptr = strstr("Hello", "world");
   }   
   wild = 0xdeadbeef;
}

// ptr is a ARR ptr
// iptr will be itype
// wild will be a wild ptr.
intfunc(int *ptr, int *iptr : itype(_Ptr<int> ) , int *wild) {/*ARR:ptr*/
   if(ptr != 0) {
      ptr[0] = 1;
   }
   wild = 0xdeadbeef;
}
int main() {
  int a, b, c;
  // this will be ARR
  _Ptr<int> ap;
  // this will be WILD
  int *bp;
  // this will be _Ptr
  _Ptr<int> cp;
  // this will be wild
  char *ap1;
  // this will be WILD
  int *bp1;
  // this will be _Ptr
  _Ptr<int> cp1;

  
  //ap1 = &a;
  ap = &a;
  // we will make this pointer wild.
  bp1 = bp = 0xcafeba;
  cp = &c;
  cp1 = &c;
  // we will make this wild in
  // main.
  ap1 = 0xdeadbe;
  // although, we are passing cp
  // to a paramter that will be 
  // treated as WILD in func
  func(ap, bp, cp);
  // ap1 will be WILD in main
  // bp1 will be WILD in main
  // cp1 will _Ptr
  funcdecl(ap1, bp1, cp1);
  
}
