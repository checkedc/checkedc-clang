// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/anonstruct.checked.c -- | count 0
// RUN: rm %S/anonstruct.checked.c

struct { 
    int *data; 
	//CHECK: _Ptr<int> data; 
} *x; 

/*ensure trivial conversion*/
void foo(int *x) { 
	//CHECK: void foo(_Ptr<int> x) _Checked { 

} 

