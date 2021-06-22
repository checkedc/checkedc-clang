//RUN: rm -rf %t*
//RUN: 3c -base-dir=%S -output-dir=%t.checked2 %s %S/extGVarbar1.c --
//RUN: FileCheck -match-full-lines --input-file %t.checked2/extGVarbar2.c %s
//RUN: %clang -working-directory=%t.checked2 -c extGVarbar2.c extGVarbar1.c

// This test cannot use pipes because it requires multiple output files

int w = 2;
int *y = &w;
//CHECK: _Ptr<int> y =  &w;

void f(int *e) {
  //ensure trivial conversion
}
//CHECK: void f(_Ptr<int> e) {
