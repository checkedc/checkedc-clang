#include <string_checked.h>
// This tests the propagation of constraints
// within the fields of structure.
typedef struct {  
  _Ptr<int> ptr;
  _Nt_arr_ptr<char> ntptr; 
} foo;

foo obj1;

int *func(_Ptr<int> ptr, _Nt_arr_ptr<char> ntptr) {
   obj1.ptr = ptr;
   obj1.ntptr = strstr(ntptr, "world");
   return ptr;
}

int main() {
  int a;
  _Ptr<int> b;
  _Nt_arr_ptr<char> wil;
  a = strlen(wil);
  b = func(&a, wil);
}
