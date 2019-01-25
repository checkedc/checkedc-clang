//
// These are regression tests cases for 
//  https://github.com/Microsoft/checkedc/issues/221
//
// The compiler crashed with an assert while checking bounds declarations. It
// had found that bounds information was already present.  The cause was that RecursiveASTVisitor
// actually visits nodes in initializer lists twice by default, despite the documentation
// saying that nodes are only visited once.  This test checks that the compiler does
// not crash and also issues an expected error message.
//
// RUN: %clang -cc1 -verify -fcheckedc-extension %s

void f(void) {
  int x;
  struct S { _Ptr<int> p_variable; };
  struct S t1 = { &x };     // crashed compiler
  struct S t2 = { 0xabcd }; // expected-error {{initializing '_Ptr<int>' with an expression of incompatible type 'int'}}
}

