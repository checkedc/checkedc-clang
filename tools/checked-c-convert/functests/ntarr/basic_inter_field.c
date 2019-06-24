#include <string_checked.h>
// This tests the propagation of constraints
// within the fields of structure.
typedef struct {  
  int *ptr;
  char *ntptr; 
} foo;

foo obj1;

int* func(int *ptr, char *ntptr) {
   obj1.ptr = ptr;
   obj1.ntptr = strstr(ntptr, "world");
   return ptr;
}

int main() {
  int a;
  int *b;
  char *wil;
  a = strlen(wil);
  b = func(&a, wil);
}
