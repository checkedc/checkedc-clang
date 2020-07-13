// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s

int *a = (int*) 1;
void b() {
  //CHECK: void b() {
  int c = *a;
}

void c() {
    int *b = (int*)1;
    { b; }
    //CHECK: _Unchecked{ b; }
}

// Dummy
void f(void) { 
  int y = 3;
  int *p = &y;

}

