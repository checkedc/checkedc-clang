//RUN: 3c -base-dir=%S -output-postfix=checked %s %S/prototype_success2.c
//RUN: FileCheck -match-full-lines --input-file %S/prototype_success1.checked.c %s
//RUN: %clang -c %S/prototype_success1.checked.c %S/prototype_success2.checked.c
//RUN: rm %S/prototype_success1.checked.c %S/prototype_success2.checked.c

/*Note: this file is part of a multi-file regression test in tandem with prototype_success2.c*/

/*prototypes that type-check with each other are fine*/
int * foo(_Ptr<int>, char); 

/*a prototype definitin combo that type-checks is also fine*/
int *bar(int *x, float *y) { 
    return x;
}

/*a C-style prototype combined with an enumerated prototype is also fine*/ 
int *baz(int);

void trivial_conversion(int *x) { 
//CHECK: void trivial_conversion(_Ptr<int> x) {
}
