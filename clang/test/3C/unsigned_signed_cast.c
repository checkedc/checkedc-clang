// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked -alltypes %s
// RUN: 3c -alltypes %S/unsigned_signed_cast.checked.c -- | count 0
// RUN: rm %S/unsigned_signed_cast.checked.c

/*unsigned integer cast*/

unsigned int *foo = 0;
	//CHECK: unsigned int *foo = 0;
int *bar = 0;  
	//CHECK: int *bar = 0;  


/*unsigned characters cast*/

unsigned char *yoo = 0; 
	//CHECK: unsigned char *yoo = 0; 
char *yar = 0; 
	//CHECK: char *yar = 0; 


/*C does not support unsigned floats, so we don't have to worry about that case*/

/*ensure trivial conversion with parameter*/
void test(int *x) {
	//CHECK: void test(_Ptr<int> x) {
  foo = bar; 
  yoo = yar;
}
