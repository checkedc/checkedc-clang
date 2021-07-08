//RUN: rm -rf %t*
//RUN: 3c -base-dir=%S -output-dir=%t.checked2 %s %S/extGVarm1.c %S/extGVarm3.c --
//RUN: FileCheck -match-full-lines --input-file %t.checked2/extGVarm2.c %s
//RUN: %clang -working-directory=%t.checked2 -c extGVarm2.c extGVarm3.c extGVarm1.c

// This test cannot use pipes because it requires multiple output files

extern int *x;
//CHECK: extern int *x;

extern int *y;
//CHECK: extern _Ptr<int> y;

int w = 4;
int *z = &w;
//CHECK: _Ptr<int> z = &w;

void g(int *y) {}
//CHECK: void g(_Ptr<int> y) {}
