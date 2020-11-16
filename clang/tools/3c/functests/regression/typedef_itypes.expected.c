typedef int* IntPtr;

int foo(IntPtr a : itype(_Ptr<int> ) ) {
  return 0;
}
IntPtr foo1(void) : itype(_Ptr<int> )  {
  _Ptr<int> a = NULL;
  return a;
}
int f() {
   int* p = (int*)0xFFFFF;
   foo(p);
   p = foo1();
   return 0;
}
int main() {
   _Ptr<int> b = NULL;
   foo(b);
   return 0;
}
