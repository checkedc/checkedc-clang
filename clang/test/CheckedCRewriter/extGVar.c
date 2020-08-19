// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/extGVar.checked.c -- | count 0
// RUN: rm %S/extGVar.checked.c

/*what we're interested in*/ 
extern int *x; 

/*safe filler to ensure that conversion happens*/ 
void g(int *y) { 
	//CHECK: void g(_Ptr<int> y) _Checked { 
	*y = 2;
}

