// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

struct { 
    int *data; 
} *x; 
//CHECK: _Ptr<int> data;
//CHECK: } *x;

/*ensure trivial conversion*/
void foo(int *x) { 

} 

