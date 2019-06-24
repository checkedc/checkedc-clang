// This tests the propagation of constraints
// within the fields of structure.
typedef struct {  
  int *ptr;
  char *ptr2; 
} foo;

foo obj1;

int* func(int *ptr, char *iwild) {
   // both the arguments are pointers
   // within function body
   obj1.ptr = ptr;
   obj1.ptr2 = iwild;
   return ptr;
}

int main() {
  int a;
  int *b;
  char *wil;
  wil = 0xdeadbeef;
  b = func(&a, wil);
}
