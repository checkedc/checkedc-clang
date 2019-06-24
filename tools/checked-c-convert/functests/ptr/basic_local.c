// basic test
// just create a regular pointer
// and access it
int main() {
  // a has to identified
  // as _Ptr
  int *a;
  // c also 
  // should be identified as 
  // _Ptr
  int *c;
  int *d;
  int b;
  a = &b;
  *a = 4;
  // this should mark d as WILD. 
  d = 0xdeadbeef;
}
