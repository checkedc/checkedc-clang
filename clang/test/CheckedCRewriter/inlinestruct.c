// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s

/*example should be set to wild by default, x is free to change*/
struct foo { 
    int *x; 
} *example; 
//CHECK: _Ptr<int> x;
//CHECK: } *example;

/*both set to wild, because x is used unsafely in a function*/
struct bar { 
    int *x; 
} *example2;  
//CHECK: int *x;
//CHECK: } *example2; 

void x() {
    example2 = 0; 
    example2->x = (int *) 5;
}

