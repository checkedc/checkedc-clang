//RUN: rm -rf %t*
//RUN: 3c -base-dir=%S -output-dir=%t.checked %s %S/extGVarm2.c %S/extGVarm3.c --
//RUN: FileCheck -match-full-lines --input-file %t.checked/extGVarm1.c %s
//RUN: %clang -working-directory=%t.checked -c extGVarm1.c extGVarm2.c extGVarm3.c

// This test cannot use pipes because it requires multiple output files

extern int *y;
//CHECK: extern _Ptr<int> y;

extern int *x;
//CHECK: extern int *x;

extern int *z;
//CHECK: extern _Ptr<int> z;

int foo() { return *z; }
