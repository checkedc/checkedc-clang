#include <string_checked.h>
// This tests the propagation of constraints
// within the fields of structure.
// we will use b as an NTArr
typedef struct {
   int a;
   char *b;
} foo;
// here we use b in a safe way as an array, hence
// it will be a array ptr
typedef struct {
  float *b;
} foo2;
// here we use p in unsafe way
// and hence will not be a safe ptr
typedef struct {
  float c;
  int *p;
  char d; 
} foo3;

int main() {
  foo obj;
  char *bp;
  float b;
  foo2 obj2;
  int hel;

  // this will make both bp and
  //  b of foo  NtARR
  obj.b = bp;
  hel = strstr(bp, "hello");

  // this will make obj2.b an array.
  obj2.b = &b;
  obj2.b++;
  // this will make obj3.p a WILD
  foo3 obj3;
  obj3.p = 0xcafebabe;
}
