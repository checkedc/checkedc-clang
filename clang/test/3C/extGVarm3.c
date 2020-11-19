//RUN: 3c -base-dir=%S -output-postfix=checked3 %s %S/extGVarm1.c %S/extGVarm2.c
//RUN: FileCheck -match-full-lines --input-file %S/extGVarm3.checked3.c %s
//RUN: rm %S/extGVarm1.checked3.c %S/extGVarm2.checked3.c %S/extGVarm3.checked3.c 

extern int *x; 
//CHECK: extern int *x;

int w = 4; 
int *y = &w;
//CHECK: _Ptr<int> y =  &w; 

extern int *z; 
//CHECK: extern _Ptr<int> z;

int * h(int *x) { 
    return z;
} 
//CHECK: _Ptr<int> h(_Ptr<int> x) {
