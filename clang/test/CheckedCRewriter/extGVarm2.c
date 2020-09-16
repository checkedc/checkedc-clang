//RUN: cconv-standalone -base-dir=%S -output-postfix=checked2 %s %S/extGVarm1.c %S/extGVarm3.c
//RUN: FileCheck -match-full-lines --input-file %S/extGVarm2.checked2.c %s
//RUN: rm %S/extGVarm1.checked2.c %S/extGVarm2.checked2.c %S/extGVarm3.checked2.c

extern int *x;
//CHECK: extern int *x; 

extern int *y; 
//CHECK: extern _Ptr<int> y;

int w = 4;
int *z = &w; 
//CHECK: _Ptr<int> z =  &w;

void g(int *y) { 

} 
//CHECK: void g(_Ptr<int> y) {