// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -output-postfix=checkedNOALL %s
// RUN: %clang -c %S/arrinlinestruct.checkedNOALL.c
// RUN: rm %S/arrinlinestruct.checkedNOALL.c

struct alpha { 
    int *data; 
};
//CHECK: _Ptr<int> data;
struct alpha *al[4];
//CHECK_ALL: _Ptr<struct alpha> al _Checked[4] = {((void *)0)};
//CHECK_NOALL: struct alpha *al[4];

/*be should be made wild, whereas a should be converted*/
struct {
  int *a;
} *be[4];  
//CHECK: _Ptr<int> a;
//CHECK: } *be[4]; 
