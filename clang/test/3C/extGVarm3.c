//RUN: rm -rf %t*
//RUN: 3c -base-dir=%S -output-dir=%t.checked3 %s %S/extGVarm1.c %S/extGVarm2.c --
//RUN: FileCheck -match-full-lines --input-file %t.checked3/extGVarm3.c %s
//RUN: %clang -working-directory=%t.checked3 -c extGVarm3.c extGVarm2.c extGVarm1.c

// This test cannot use pipes because it requires multiple output files

extern int *x;
//CHECK: extern int *x;

int w = 4;
int *y = &w;
//CHECK: _Ptr<int> y = &w;

extern int *z;
//CHECK: extern _Ptr<int> z;

int *h(int *x) { return z; }
//CHECK: _Ptr<int> h(_Ptr<int> x) { return z; }
