// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone -output-postfix=checked %s 
// RUN: %clang -c %S/refarrsubscriptstruct.checked.c
// RUN: rm %S/refarrsubscriptstruct.checked.c

struct foo { int **b; };
int **bar(struct foo *p) {
  return &(p->b[1]);
} 
//CHECK: struct foo { int **b; };
//CHECK: int ** bar(_Ptr<struct foo> p) {