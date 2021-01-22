// Tests for 3C.
//
// Tests properties about constraint propagation between functions.
//
// RUN: 3c %s -- | FileCheck -match-full-lines %s
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
  // we are passing cp and cp1
  // to a paramter that will be
  // treated as WILD in callee, which
  // forces it to be WILD in main
  func(ap, bp, cp);
  funcdecl(ap1, bp1, cp1);
  return 0;
}
//CHECK: _Ptr<int> ap =  0;
//CHECK: _Ptr<int> ap1 =  0;
