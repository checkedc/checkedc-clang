// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/i1.c -- | diff %t.checked/i1.c -

/*nothing in this file, save the trivially converted w, should be converted*/
int *foo(int *w) {
  //CHECK: int *foo(_Ptr<int> w) : itype(_Ptr<int>) {
  int x = 1;
  int y = 2;
  int z = 3;
  int *ret;
  //CHECK: int *ret;
  for (int i = 0; i < 4; i++) {
    //CHECK: for (int i = 0; i < 4; i++) _Checked {
    switch (i) {
      //CHECK: switch (i) _Unchecked {
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
      //CHECK: ret = (int *)5;
      break;
    }
  }
  return ret;
}
