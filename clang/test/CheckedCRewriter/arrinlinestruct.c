// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/arrinlinestruct.checked.c -- | count 0
// RUN: rm %S/arrinlinestruct.checked.c

struct alpha { 
    int *data; 
	//CHECK: _Ptr<int> data; 
};
struct alpha *al[4];
	//CHECK_NOALL: struct alpha *al[4];
	//CHECK_ALL: _Ptr<struct alpha> al _Checked[4];

/*be should be made wild, whereas a should be converted*/
struct {
  int *a;
	//CHECK: _Ptr<int> a;
} *be[4];  
