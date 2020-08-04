// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s

int foo(int x) { 
  //CHECK: int foo(int x) {
  x += non();
  return x;
}


// Dummy to cause output
int dummy(int *x) { 
  return *x;
}
