// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone -output-postfix=checked %s 
// RUN: %clang -c %S/refarrsubscript.checked.c
// RUN: rm %S/refarrsubscript.checked.c

int **foo(int **p, int *x) {
  return &(p[1]);
} 
//CHECK: int ** foo(int **p, _Ptr<int> x) {

