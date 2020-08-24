// RUN: cconv-standalone -addcr -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone --addcr --alltypes %s -- | %clang_cc1  -fcheckedc-extension -x c -

int bar(void) { 
  //CHECK: int bar(void) _Checked {
  int local[10];
  //CHECK: int local _Checked[10];
  int (*coef)[10] = &local;
  //CHECK: _Ptr<int _Checked[10]> coef = &local;
  return (*coef)[1];
}

