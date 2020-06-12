// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s

struct alpha { 
    int *data; 
};
//CHECK: _Ptr<int> data;
struct alpha *al[4];
//CHECK: _Ptr<struct alpha> al _Checked[4]; 

/*be should be made wild, whereas a should be converted*/
struct {
  int *a;
} *be[4];  
//CHECK: _Ptr<int> a;
//CHECK: } *be[4]; 
