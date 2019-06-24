// This tests the propagation of constraints
// within the fields of structure.
typedef struct {  
  _Ptr<int> ptr;
  _Ptr<char> ptr2; 
} foo;

foo obj1;

int *func(_Ptr<int> ptr, char *iwild : itype(_Ptr<char> ) ) {
   // both the arguments are pointers
   // within function body
   obj1.ptr = ptr;
   obj1.ptr2 = iwild;
   return ptr;
}

int main() {
  int a;
  _Ptr<int> b;
  char *wil;
  wil = 0xdeadbeef;
  b = func(&a, wil);
}
