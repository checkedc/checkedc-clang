//
// This example is from https://github.com/Microsoft/checkedc-clang/issues/143
//
// RUN: %clang -c -fcheckedc-extension %s -o %t

void f1(int *p : itype(_Ptr<int>)) {
}

void g1(_Ptr<int> p) {
  f1(p);
}
