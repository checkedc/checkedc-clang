//RUN: 3c -base-dir=%S -output-postfix=checked2 %s %S/prototype_success1.c
//RUN: FileCheck -match-full-lines --input-file %S/prototype_success2.checked2.c %s
//RUN: rm %S/prototype_success1.checked2.c %S/prototype_success2.checked2.c 

/*Note: this file is part of a multi-file regression test in tandem with prototype_success1.c 
For comments about the different functions in this file, please refer to prototype_success1.c*/ 

_Ptr<int> foo(int *, char); 

int *bar(int *, float *); 

int *baz(); 

void trivial_conversion2(int *x) { 
//CHECK: void trivial_conversion2(_Ptr<int> x) {
}