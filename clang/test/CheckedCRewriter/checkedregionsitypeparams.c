// RUN: cconv-standalone -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s

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
    // CHECK:if(1) _Unchecked {
    return foo(a);
  } else { 
    return 3;
  }

}
