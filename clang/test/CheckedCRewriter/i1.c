// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

/*nothing in this file, save the trivially converted w, should be converted*/
int *foo(int *w) {
  //CHECK: int * foo(_Ptr<int> w) {
  int x = 1;
  int y = 2;
  int z = 3;
  int *ret;
  for (int i = 0; i < 4; i++) {
    switch(i) {
    case 0:
      ret = &x;
      break;
    case 1:
      ret = &y;
      break;
    case 2:
      ret = &z;
      break;
    case 3:
      ret = (int *)5;
      break;
    }
  }
  return ret;
}
