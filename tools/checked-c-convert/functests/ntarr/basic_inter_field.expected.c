#include <string_checked.h>
// This tests the propagation of constraints
// within the fields of structure.
typedef struct {  
  _Ptr<int> ptr;
  _Nt_array_ptrchar> ntptr; 
} foo;

foo obj1;

_Ptr<int> func(_Ptr<int> ptr, _Nt_array_ptrchar> ntptr) {
   obj1.ptr = ptr;
   obj1.ntptr = strstr(ntptr, "world");
   return ptr;
}

int main() {
  int a;
  _Ptr<int> b;
  _Nt_array_ptrchar> wil;
  a = strlen(wil);
  b = func(&a, wil);
}
