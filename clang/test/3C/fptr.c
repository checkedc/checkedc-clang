// RUN: 3c -addcr %s -- -fcheckedc-extension | FileCheck -match-full-lines %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

int foo(char *c) {
  c = (char*) 3;
  return 2;
}

char* bar(int x) { 
  return (char*) x;
}

int f(char c) { 
  //CHECK: int f(char c) {
  int (*x)(char*) = foo;
  //CHECK: _Ptr<int (char *)> x = foo;

  return 0;
}

