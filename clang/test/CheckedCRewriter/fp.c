// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/fp.checked.c -- | count 0
// RUN: rm %S/fp.checked.c


int f(int *p);
	//CHECK: int f(int *p);
void bar() {
  int (*fp)(int *p) = f;
	//CHECK: _Ptr<int (int *)> fp =  f;
  f((void*)0);
}

int mul_by_2(int x) { 
	//CHECK: int mul_by_2(int x) _Checked { 
    return x * 2;
}

int (*foo(void)) (int) {
	//CHECK: _Ptr<int (int )> foo(void) _Checked {
    return mul_by_2;
} 

