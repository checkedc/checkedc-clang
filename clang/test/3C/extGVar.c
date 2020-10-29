// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked -alltypes %s
// RUN: 3c -alltypes %S/extGVar.checked.c -- | count 0
// RUN: rm %S/extGVar.checked.c

/*what we're interested in*/ 
extern int *x; 

/*safe filler to ensure that conversion happens*/ 
void g(int *y) { 
	//CHECK: void g(_Ptr<int> y) _Checked { 
	*y = 2;
}

