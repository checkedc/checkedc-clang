// Tests for 3C.
//
// Tests basic rewriting of Nt_array_ptrs

// RUN: 3c -alltypes %s -- | FileCheck -match-full-lines %s
//

unsigned long strlen(const char *s : itype(_Nt_array_ptr<const char>));
char *strstr(const char *s1 : itype(_Nt_array_ptr<const char>),
             const char *s2 : itype(_Nt_array_ptr<const char>)) : itype(_Nt_array_ptr<char>);

// basic test
// just create a NT pointer
int main() {
  char *a = 0;
  char *c = 0;
  int *d = 0;
  int b;
  // this will make a as NTARR
  b = strlen(a);
  // this will make C as NTArr
  c = strstr("Hello", "World");
  // this should mark d as WILD.
  d = (int*)0xdeadbeef;
  return 0;
}

//CHECK: int main() {
//CHECK-NEXT: _Nt_array_ptr<char> a =  0;
//CHECK-NEXT: _Ptr<char> c =  0;
//CHECK-NEXT: int *d = 0;
