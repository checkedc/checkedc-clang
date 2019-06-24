// This tests the propagation of constraints
// within the fields of structure.
// we will use b as an ARR
typedef struct {
   int a;
   int *b;
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
  // this will be ARR
  int *bp;
  float b;
  foo2 obj2;
  int hel;

  // this will make field b of foo an ARR
  obj.b = bp;
  bp = &hel;
  bp[0] = 1;

  // this will make obj2.b an array.
  obj2.b = &b;
  obj2.b++;

  foo3 obj3;
  obj3.p = 0xcafebabe;
}
