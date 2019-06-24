// basic test
// just create a regular arr pointer
// and access it
int main() {
  // a has to identified
  // as ARR
  int *a;
  // c also should be identified as 
  // _Ptr as we do not use it.
  int *c;
  // we will make this wild.
  int *d;
  int b;
  a = &b;
  // this will make a as ARR
  a++;
  // this should mark d as WILD. 
  d = 0xdeadbeef;
}
