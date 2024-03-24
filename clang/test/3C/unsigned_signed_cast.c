// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/unsigned_signed_cast.c -- | diff %t.checked/unsigned_signed_cast.c -

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

/*C does not support unsigned floats, so we don't have to worry about that
  case*/

/*ensure trivial conversion with parameter*/
void test(int *x) {
  //CHECK: void test(_Ptr<int> x) {
  foo = bar;
  yoo = yar;
}
