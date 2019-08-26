// Tests for Checked C rewriter tool.
//
// Tests properties about constraint propagation between functions.
//
// RUN: checked-c-convert %s -- | FileCheck -match-full-lines %s
//

int funcdecl(int *ptr, int *iptr, int *wild);
int funcdecl(int *ptr, int *iptr, int *wild) {
  if(ptr != 0) {
    *ptr = 0;
  }
  wild = (int*)0xdeadbeef;
  return 0;
}
//CHECK: int funcdecl(_Ptr<int> ptr, int *iptr : itype(_Ptr<int>), int *wild);
//CHECK-NEXT: int funcdecl(_Ptr<int> ptr, int *iptr : itype(_Ptr<int>), int *wild) {

// ptr is a regular _Ptr
// iptr will be itype
// wild will be a wild ptr.
int func(int *ptr, int *iptr, int *wild) {
  if(ptr != 0) {
    *ptr = 0;
  }
  wild = (int*)0xdeadbeef;
  return 0;
}
//CHECK: int func(_Ptr<int> ptr, int *iptr : itype(_Ptr<int>), int *wild) {

int main() {
  int a, b, c;
  // this will be _Ptr
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
  // although, we are passing cp
  // to a paramter that will be
  // treated as WILD in func, cp
  // is Ptr within main
  func(ap, bp, cp);
  funcdecl(ap1, bp1, cp1);
  return 0;
}
//CHECK: _Ptr<int> ap =  0;
//CHECK-NEXT: int *bp = 0;
//CHECK-NEXT: _Ptr<int> cp =  0;
//CHECK-NEXT: _Ptr<int> ap1 =  0;
//CHECK-NEXT: int *bp1 = 0;
//CHECK-NEXT: _Ptr<int> cp1 =  0;


