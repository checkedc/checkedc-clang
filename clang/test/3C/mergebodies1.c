// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checked %s %S/mergebodies2.c -- -Wno-error=int-conversion
// RUN: %clang -working-directory=%t.checked -c mergebodies1.c mergebodies2.c -Wno-error=int-conversion
// RUN: FileCheck -match-full-lines --input-file %t.checked/mergebodies1.c %s
// RUN: 3c -base-dir=%t.checked -output-dir=%t.convert_again %t.checked/mergebodies1.c %t.checked/mergebodies2.c -- -Wno-error=int-conversion
// RUN: test ! -f %t.convert_again/mergebodies1.c

// This test and its counterpart check for merging functions with bodies,
// differing number of parameters, renamed parameters, and multiple mains
// Note that the name added to a parameter without it may differ based on
// the order of parsing files.

int safeBody(int a, int *b);
// CHECK: int safeBody(int a, _Ptr<int> b);

int safeBody(int,int*);
// CHECK: int safeBody(int x, _Ptr<int> y);

int safeBody();
// CHECK: int safeBody(int x, _Ptr<int> y);

int safeBody(int x, int *y) {
// CHECK: int safeBody(int x, _Ptr<int> y) _Checked {
  return x + *y;
}

int *wildBody(int x) {
// CHECK: int *wildBody(int x) : itype(_Ptr<int>) {
  return (int *)x;
};

int *fnPtrParamUnsafe(int (*)(int,int), int(*)(int*));
// CHECK: int *fnPtrParamUnsafe(int ((*fn)(int, int)) : itype(_Ptr<int (int, int)>), _Ptr<int (_Ptr<int>)>) : itype(_Ptr<int>);

int *fnPtrParamSafe(int (*)(int,int));
// CHECK: int *fnPtrParamSafe(_Ptr<int (int, int)> fn) : itype(_Ptr<int>);

void main() {
// CHECK: void main(int a, _Ptr<_Ptr<char>> b) {
  int f = safeBody(1,(int *)2);
}

void main(int a, char **b);
// CHECK: void main(int a, _Ptr<_Ptr<char>> b);

int *noParams() {
  return 0;
}
// CHECK: _Ptr<int> noParams(void) _Checked {
