// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

struct A { 
  int b;
};

int foo(struct A *a) { 
  //CHECK: int foo(struct A *a : itype(_Ptr<struct A>)) _Checked {
  return a->b + 1;
}

int bar(struct A *a) { 
//CHECK: int bar(struct A *a) {
  a = (struct A*) 5;
  if(1) { 
    // CHECK:if(1) {
    return foo(a);
  } else { 
    // CHECK: } else _Checked {
    return 3;
  }

}
