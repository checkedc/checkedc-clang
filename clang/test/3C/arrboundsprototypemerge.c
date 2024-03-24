// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -f3c-tool -fcheckedc-extension -x c -o %t1.unused -

/*
Handle function prototype merging in bounds inference.
https://github.com/correctcomputation/checkedc-clang/issues/522
*/

#include <string_checked.h>

void fiddle_with_strings(const char *tc) {
  strdup(tc);
  strcmp(tc, tc);
  strcmp(tc, "some really long string");
}
//CHECK: void fiddle_with_strings(_Nt_array_ptr<const char> tc) {

void get_string_from_callback(const char *(*callback)(void)) {
  const char *p = callback();
  strdup(p);
}
//CHECK: void get_string_from_callback(_Ptr<_Nt_array_ptr<const char> (void)> callback) {
//CHECK:   _Nt_array_ptr<const char> p = callback();
