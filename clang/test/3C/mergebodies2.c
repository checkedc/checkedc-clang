// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checked %s %S/mergebodies1.c -- -Wno-error=int-conversion
// RUN: %clang -working-directory=%t.checked -c mergebodies2.c mergebodies1.c -Wno-error=int-conversion
// RUN: FileCheck -match-full-lines --input-file %t.checked/mergebodies2.c %s
// RUN: 3c -base-dir=%t.checked -output-dir=%t.convert_again %t.checked/mergebodies2.c %t.checked/mergebodies1.c -- -Wno-error=int-conversion
// RUN: test ! -f %t.convert_again/mergebodies2.c

// This test and its counterpart check for merging functions with bodies,
// differing number of parameters, renamed parameters, and multiple mains
// Note that the name added to a parameter without it may differ based on
// the order of parsing files.

int safeBody(int a, int *b) {
// CHECK: int safeBody(int a, _Ptr<int> b) _Checked {
  return a;
}

int *wildBody() {
// CHECK: int *wildBody(int x, _Ptr<int> y, _Ptr<_Ptr<int>> z) : itype(_Ptr<int>) {
  return (int *)1;
};

int *wildBody(int x, int *y, int **z);
// CHECK: int *wildBody(int x, _Ptr<int> y, _Ptr<_Ptr<int>> z) : itype(_Ptr<int>);

int *fnPtrParamUnsafe(int (*fn)(int,int)) {
// CHECK: int *fnPtrParamUnsafe(int ((*fn)(int, int)) : itype(_Ptr<int (int, int)>)) : itype(_Ptr<int>) {
  fn = (int (*)(int,int))5;
  return fn(1,3);
}

int *fnPtrParamSafe(int (*fn)(int,int)) {
// CHECK: int *fnPtrParamSafe(_Ptr<int (int, int)> fn) : itype(_Ptr<int>) _Checked {
  return fn(0,0);
}

int main(int argc, char** argv) {
// CHECK: int main(int argc, _Ptr<_Ptr<char>> argv) _Checked {
  return 0;
}
