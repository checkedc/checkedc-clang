// This is a regression test case for
//  https://github.com/checkedc/checkedc-llvm-project/issues/1200
//
// This test checks conversion of a pointer-typed variable with a
// bounds-safe interface to an unchecked pointer type, wehere the
// bound-safe interface type uses a typedef'ed type.
//
// RUN: %clang -cc1 -verify -fcheckedc-extension %s
// expected-no-diagnostics

typedef char CHAR;

int
FindAccessVariable (_Nt_array_ptr<char> VariableName) {
  return -1;
}

int
mineVariableServiceGetVariable (
char  *VariableName : itype(_Nt_array_ptr<CHAR>)) {
return FindAccessVariable (VariableName);
}
