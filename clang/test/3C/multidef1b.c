// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes -output-dir=%t.checkedALL %s %S/multidef1a.c --
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checkedNOALL %s %S/multidef1a.c --
// RUN: %clang -working-directory=%t.checkedNOALL -c multidef1a.c multidef1b.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" --input-file %t.checkedNOALL/multidef1b.c %s
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" --input-file %t.checkedALL/multidef1b.c %s

#include <string.h>

int main(int argc, char **argv) {
//CHECK_NOALL: int main(int argc, char **argv : itype(_Ptr<_Ptr<char>>)) {
//CHECK_ALL: int main(int argc, _Array_ptr<_Nt_array_ptr<char>> argv : count(argc)) _Checked {
  if (argc > 1) {
    int x = strlen(argv[1]);
    return x;
  }
  return 0;
}

int foo(int argc, char **argv) {
//CHECK_NOALL: int foo(int argc, char **argv : itype(_Ptr<_Ptr<char>>)) {
//CHECK_ALL: int foo(int argc, _Array_ptr<_Nt_array_ptr<char>> argv : count(0 + 1)) _Checked {
  if (argc > 1) {
    int x = strlen(argv[1]);
    return x;
  }
  return 0;
}

