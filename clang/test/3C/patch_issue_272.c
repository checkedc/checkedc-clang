// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -f3c-tool -fcheckedc-extension -x c -o %t.unused -

/********************************************************************/
/* Tests to keep pointer level from                                 */
/* https://github.com/correctcomputation/checkedc-clang/issues/272  */
/********************************************************************/

int *a[];
void b() { a[0][0]; }

// CHECK: _Array_ptr<int> a _Checked[] = {((void *)0)};
