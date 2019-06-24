// This tests the propagation of constraints
// within the fields of structure.
typedef struct {  
  // regular ptr
  int *ptr;
  // this will be array ptr
  char *arrptr; 
} foo;

foo obj1;

int* func(int *ptr, char *arrptr) {
   obj1.ptr = ptr;
   arrptr++;
   obj1.arrptr = arrptr;
   return ptr;
}

int main() {
  int a;
  int *b;
  char *wil;
  wil = 0xdeadbeef;
  b = func(&a, wil);
}
