// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes -output-dir=%t.checkedALL %s %S/multidef1b.c --
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checkedNOALL %s %S/multidef1b.c --
// RUN: %clang -working-directory=%t.checkedNOALL -c multidef1a.c multidef1b.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" --input-file %t.checkedNOALL/multidef1a.c %s
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" --input-file %t.checkedALL/multidef1a.c %s

int main(int argc, char **argv) {
//CHECK_NOALL: int main(int argc, char **argv : itype(_Ptr<_Ptr<char>>)) {
//CHECK_ALL: int main(int argc, _Array_ptr<_Nt_array_ptr<char>> argv : count(argc)) _Checked {
  if (argc > 1)
    return 1;
  else
    return 0;
}

int foo(int argc, char **argv) {
//CHECK_ALL: int foo(int argc, _Array_ptr<_Nt_array_ptr<char>> argv : count(0 + 1)) _Checked {
//CHECK_NOALL: int foo(int argc, char **argv : itype(_Ptr<_Ptr<char>>)) {
  char p = argv[0][0];
  return 0;
}

