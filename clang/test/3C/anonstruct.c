// RUN: 3c -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

struct { 
    int *data; 
} *x; 
//CHECK: _Ptr<int> data;
//CHECK: } *x;

/*ensure trivial conversion*/
void foo(int *x) { 

} 

