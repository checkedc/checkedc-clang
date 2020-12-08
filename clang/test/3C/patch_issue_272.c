// RUN: 3c -alltypes %s | FileCheck -match-full-lines %s

/********************************************************************/
/* Tests to keep pointer level from                                 */
/* https://github.com/correctcomputation/checkedc-clang/issues/272  */
/*                                                                  */
/* Currently not possible to run clang on the output,               */
/* since 3c cannot yet determine full array bounds in this case     */
/*                                                                  */
/********************************************************************/

int *a[];
void b() { a[0][0]; }

// CHECK: _Array_ptr<int> a _Checked[1] = {((void *)0)};
