// Tests to make sure generic function call errors are parsed correctly.
//
// More specifically, we are testing for below cases of error :
// 1) Using generic function call syntax on normal function will not be
//    allowed. 
// 2) a special case that I found. Description below.
//
// RUN: %clang_cc1 -fcheckedc-extension -verify %s

_For_any(T) T *Foo(T *a, T *b) {
  return a;
}

void Bar() {
  // There will be parsing error here, since you cannot create a variable of
  // type void. This results in the Expr representation of Foo to be NULL for
  // some reason. Since I use isa<DeclRefExpr>() in the code, assert kicks in
  // saying you cannot do isa<> on NULL pointer, crashing compiler. I fixed the
  // code to check whether the Expr* is NULL or not.
  void* x, y; //expected-error{{variable has incomplete type 'void'}}
  Foo<void>(x, y);
  return;
}

void CallGenericFunction() {
  // Although it's possible to create a separate check and error message to say
  // "Bar is not a generic function, so we cannot have this syntax," it seems 
  // sufficient enough for the clang to generate error message as if generic 
  // functions were never implemented.
  Bar<unsigned int>(); //expected-error{{expected expression}}
}
