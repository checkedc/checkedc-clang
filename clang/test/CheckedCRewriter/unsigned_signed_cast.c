// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -output-postfix=checkedNOALL %s
// RUN: %clang -c %S/unsigned_signed_cast.checkedNOALL.c
// RUN: rm %S/unsigned_signed_cast.checkedNOALL.c

/*unsigned integer cast*/

unsigned int *foo = 0;
int *bar = 0;  

//CHECK: unsigned int *foo = 0;
//CHECK: int *bar = 0;  

/*unsigned characters cast*/

unsigned char *yoo = 0; 
char *yar = 0; 

//CHECK: unsigned char *yoo = 0; 
//CHECK: char *yar = 0; 

/*C does not support unsigned floats, so we don't have to worry about that case*/

/*ensure trivial conversion with parameter*/
void test(int *x) {
  foo = bar; 
  yoo = yar;
}