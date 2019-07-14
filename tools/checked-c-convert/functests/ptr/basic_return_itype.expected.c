// here we test the propagation of constraints
// between functions.
static int funcvar;
static int funcdecvar;
// we test propagation with and without function
// declaration.
static int *funcdecl(_Ptr<int> ptr, int *iptr : itype(_Ptr<int> ) , int *wild) : itype(_Ptr<int> ) ;
static int *funcdecl(_Ptr<int> ptr, int *iptr : itype(_Ptr<int> ) , int *wild) : itype(_Ptr<int> )  {
   if(ptr != 0) {
    *ptr = 0;
   }   
   wild = 0xdeadbeef;
   return &funcdecvar;
}

// ptr is a regular _Ptr
// iptr will be itype
// wild will be a wild ptr.
static int *func(_Ptr<int> ptr, int *iptr : itype(_Ptr<int> ) , int *wild) : itype(_Ptr<int> )  {
   if(ptr != 0) {
    *ptr = 0;
   }
   wild = 0xdeadbeef;
   return &funcvar;
}
int main() {
  int a, b, c;
  // this will be _Ptr
  _Ptr<int> ap;
  // this will be WILD
  int *bp;
  // this will be _Ptr
  _Ptr<int> cp;
  // this will be _Ptr
  _Ptr<int> ap1;
  // this will be WILD
  int *bp1;
  // this will be _Ptr
  _Ptr<int> cp1;

  
  ap1 = ap = &a;
  // we will make this pointer wild.
  bp1 = bp = 0xcafeba;
  cp = &c;
  cp1 = &c;
  // although, we are passing cp
  // to a paramter that will be 
  // treated as WILD in func, cp
  // is Ptr within main
  bp = func(ap, bp, cp);
  bp1 = funcdecl(ap1, bp1, cp1);
}
