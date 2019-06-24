// This tests the propagation of constraints
// within the fields of structure.
// we do not use b so it will be a _Ptr
typedef struct {
   int a;
   int *b;
} foo;
// here we use b in a safe way, hence
// it will be a _Ptr
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
  float b;
  foo2 obj2;
  obj2.b = &b;
  foo3 obj3;
  obj3.p = 0xcafebabe;
}
