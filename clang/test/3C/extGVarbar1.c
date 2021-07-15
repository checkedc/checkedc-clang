//RUN: rm -rf %t*
//RUN: 3c -base-dir=%S -output-dir=%t.checked %s %S/extGVarbar2.c --
//RUN: FileCheck -match-full-lines --input-file %t.checked/extGVarbar1.c %s
//RUN: %clang -working-directory=%t.checked -c extGVarbar1.c extGVarbar2.c

// This test cannot use pipes because it requires multiple output files

extern int *x;
/*y will be defined in bar2.c*/
extern int *y;

//CHECK: extern int *x;
//CHECK: extern _Ptr<int> y;

/*trivial conversion guarantee*/
void g(int *y) {}
//CHECK: void g(_Ptr<int> y) {}
