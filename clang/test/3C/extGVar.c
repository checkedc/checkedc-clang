// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/extGVar.c -- | diff %t.checked/extGVar.c -

/*what we're interested in*/
extern int *x;

/*safe filler to ensure that conversion happens*/
void g(int *y) {
  //CHECK: void g(_Ptr<int> y) _Checked {
  *y = 2;
}
