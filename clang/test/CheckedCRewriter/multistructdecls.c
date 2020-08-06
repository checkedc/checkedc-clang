// RUN: cconv-standalone %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -


struct foo {
    int *r, *l;
};

//CHECK: _Ptr<int> r; _Ptr<int> l; 

void bar(void) {
    struct foo *p = 0;
    //CHECK: _Ptr<struct foo> p = 0;
}

