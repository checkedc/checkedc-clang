//
// This is a regression test case for 
// https://github.com/Microsoft/checkedc-clang/issues/526
//
// The compiler was crashing with an internal assertion that an expression
// to which a bounds check was going to be added already had a bounds 
// check.
//
// The problem is that the compiler was traversing the 1st argument of a
// _Dynamic_bounds_cast operation twice.  The compiler inferred bounds
// that use the 1st argument.  It then attached them to the AST.  There
// was a lack of clarity in the IR and the inferred bounds were also
// traversed, causing the assert.
//
// RUN: %clang -cc1 -verify %s
// expected-no-diagnostics

struct obj {
  _Array_ptr<_Nt_array_ptr<char>> names : count(len);
  unsigned int len;
};

void f(const struct obj *object ) {
   unsigned int i = 0;
   _Nt_array_ptr<const char> t : count(0) = 
      _Dynamic_bounds_cast<_Nt_array_ptr<const char>>(object->names[i], 
                                                      count(0));
 }

