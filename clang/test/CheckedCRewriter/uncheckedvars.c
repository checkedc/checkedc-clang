// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s

int *a = (int*) 1;
//CHECK: int *a = (int*) 1;
void b() {
  //CHECK: void b() {
  int c = *a;
}

void c() {
//CHECK: void c() {
    int *b = (int*)1;
    //CHECK: int *b = (int*)1;
    { b; }
    //CHECK: { b; }
}

// Dummy
void f(void) { 
//CHECK: void f(void) _Checked {
  int y = 3;
  int *p = &y;

}

