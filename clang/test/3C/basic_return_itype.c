// Tests for 3C.
//
// Tests properties about rewriter for return type when it is an itype
//
// RUN: 3c -alltypes %s -- | FileCheck -match-full-lines %s
//

static int funcvar;
static int funcdecvar;
// we test propagation with and without function
// declaration.
static int* funcdecl(int *ptr, int *iptr, int *wild);
static int* funcdecl(int *ptr, int *iptr, int *wild) {
  if(ptr != 0) {
    *ptr = 0;
  }
  wild = (int*)0xdeadbeef;
  return &funcdecvar;
}
//CHECK: static int *funcdecl(_Ptr<int> ptr, int *iptr : itype(_Ptr<int>), int *wild) : itype(_Ptr<int>);
//CHECK-NEXT: static int *funcdecl(_Ptr<int> ptr, int *iptr : itype(_Ptr<int>), int *wild) : itype(_Ptr<int>) {

// ptr is a regular _Ptr
// iptr will be itype
// wild will be a wild ptr.
static int* func(int *ptr, int *iptr, int *wild) {
  if(ptr != 0) {
    *ptr = 0;
  }
  wild = (int*)0xdeadbeef;
  return &funcvar;
}
//CHECK: static int *func(_Ptr<int> ptr, int *iptr : itype(_Ptr<int>), int *wild) : itype(_Ptr<int>) {

int main() {
  int a, b, c;
  int *ap = 0;
  int *bp = 0;
  int *cp = 0;
  int *ap1 = 0;
  int *bp1 = 0;
  int *cp1 = 0;


  ap1 = ap = &a;
  // we will make this pointer wild.
  bp1 = bp = (int*)0xcafeba;
  cp = &c;
  cp1 = &c;
  // we are passing cp
  // to a parameter that will be
  // treated as WILD in func, so it
  // must also be, in main
  bp = func(ap, bp, cp);
  bp1 = funcdecl(ap1, bp1, cp1);
  return 0;
}
//CHECK: int main() {
//CHECK-NEXT: int a, b, c;
//CHECK-NEXT: _Ptr<int> ap =  0;
//CHECK-NEXT: int *bp = 0;
//CHECK-NEXT: int *cp =  0;
//CHECK-NEXT: _Ptr<int> ap1 =  0;
//CHECK-NEXT: int *bp1 = 0;
//CHECK-NEXT: int *cp1 =  0;
