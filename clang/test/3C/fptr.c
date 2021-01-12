// RUN: 3c -addcr %s -- -fcheckedc-extension | FileCheck -match-full-lines %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

int foo(char *c) {
//CHECK: int foo(char *c : itype(_Ptr<char>)) {
  c = (char*) 3;
  return 2;
}

char* bar(int x) { 
// CHECK: char *bar(int x) : itype(_Ptr<char>) { 
  return (char*) x;
}

int f(char c) { 
  //CHECK: int f(char c) _Checked {
  int (*x)(char*) = &foo;
  //CHECK: _Ptr<int (char * : itype(_Ptr<char>))> x = &foo;

  return 0;
}

