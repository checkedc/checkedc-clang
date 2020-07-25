//RUN: cconv-standalone -base-dir=%S -output-postfix=checked %s %S/extGVarm2.c %S/extGVarm3.c
//RUN: FileCheck -match-full-lines --input-file %S/extGVarm1.checked.c %s
//RUN: %clang -c %S/extGVarm1.checked.c %S/extGVarm2.checked.c %S/extGVarm3.checked.c
//RUN: rm %S/extGVarm1.checked.c %S/extGVarm2.checked.c %S/extGVarm3.checked.c

extern int *y; 
//CHECK: extern _Ptr<int> y; 

extern int *x; 
//CHECK: extern int *x;

extern int *z; 
//CHECK: extern _Ptr<int> z; 

int foo() { 
    return *z;
}