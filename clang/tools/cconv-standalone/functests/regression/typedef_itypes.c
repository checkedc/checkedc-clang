typedef int* IntPtr;

int foo(IntPtr a) {
  return 0;
}
IntPtr foo1() {
  int *a;
  return a;
}
int f() {
   int* p = (int*)0xFFFFF;
   foo(p);
   p = foo1();
   return 0;
}
int main() {
   int *b;
   foo(b);
   return 0;
}
