//RUN: rm -rf %t*
//RUN: 3c -base-dir=%S -output-dir=%t.checked2 %s %S/prototype_success1.c --
//RUN: FileCheck -match-full-lines --input-file %t.checked2/prototype_success2.c %s
//RUN: %clang -working-directory=%t.checked2 -c prototype_success1.c prototype_success2.c

/*Note: this file is part of a multi-file regression test in tandem with
  prototype_success1.c. For comments about the different functions in this file,
  please refer to prototype_success1.c*/

_Ptr<int> foo(int *, char);

int *bar(int *, float *);

int *baz();

_Ptr<int> yoo(char *x, float y, int **z);

void trivial_conversion2(int *x) {
  //CHECK: void trivial_conversion2(_Ptr<int> x) {
}
