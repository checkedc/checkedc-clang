// This tests the propagation of constraints
// within the fields of structure.
typedef struct {  
  // regular ptr
  _Ptr<int> ptr;
  // this will be array ptr
  char *arrptr; 
} foo;

foo obj1;

_Ptr<int> func(_Ptr<int> ptr, char *arrptr) {
   obj1.ptr = ptr;
   arrptr++;
   obj1.arrptr = arrptr;
   return ptr;
}

int main() {
  int a;
  _Ptr<int> b = NULL;
  char *wil;
  wil = 0xdeadbeef;
  b = func(&a, wil);
}
