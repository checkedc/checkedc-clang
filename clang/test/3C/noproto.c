// RUN: 3c -addcr %s | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: 3c -addcr %s | %clang -c -fcheckedc-extension -x c -o /dev/null -

int foo(int x) { 
  //CHECK: int foo(int x) {
  x += non();
  return x;
}


// Dummy to cause output
int dummy(int *x) { 
  return *x;
}
