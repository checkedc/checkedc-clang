//RUN: 3c -base-dir=%S -output-postfix=checked %s %S/extGVarbar2.c
//RUN: FileCheck -match-full-lines --input-file %S/extGVarbar1.checked.c %s
//RUN: %clang -c %S/extGVarbar1.checked.c %S/extGVarbar2.checked.c
//RUN: rm %S/extGVarbar1.checked.c %S/extGVarbar2.checked.c

/*first of the bar files*/ 

extern int *x; 
/*y will be defined in bar2.c*/
extern int *y;  

//CHECK: extern int *x; 
//CHECK: extern _Ptr<int> y;

/*trivial conversion guarantee*/
void g(int *y) { 

}
//CHECK: void g(_Ptr<int> y) { 
