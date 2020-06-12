// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s

struct { 
    int *data; 
} *x; 
//CHECK: _Ptr<int> data;
//CHECK: } *x;

/*ensure trivial conversion*/
void foo(int *x) { 

} 

