//RUN: cconv-standalone -base-dir=%S -output-postfix=checked2 %s %S/extGVarbar1.c
//RUN: FileCheck -match-full-lines --input-file %S/extGVarbar2.checked2.c %s
//RUN: rm %S/extGVarbar1.checked2.c %S/extGVarbar2.checked2.c

/*second of the bar files*/ 

int w = 2;
int *y = &w; 
//CHECK: _Ptr<int> y =  &w; 

void f(int *e) { 
    //ensure trivial conversion
}
//CHECK: void f(_Ptr<int> e) {