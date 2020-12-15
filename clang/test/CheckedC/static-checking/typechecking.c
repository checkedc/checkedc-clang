// Tests for clang-specific tests of typechecking of Checked C
// extensions.  It includes clang-specific error messages as well
// tests of clang-specific extensions.
//
// The Checked C repo contains many tests of typechecking as part
// of its extension conformance test suite that also check clang error
// messages.  The extension conformance tests are designed to test overall
// compiler compliance with the Checked C specification.  This file is
// for more detailed tests of error messages, such as notes and correction 
// hints emitted as part of clang diagnostics.
//
// RUN: %clang_cc1 -verify -fcheckedc-extension %s

// Prototype of a function followed by an old-style K&R definition
// of the function.

// The Checked C specification does not allow no prototype functions to have
// return types that are checked types.  Technically, the K&R style function
// definition is a no prototype function, so we could say it is illegal.
// However, clang enforces the prototype declaration at the definition of
// f100, so this seems OK to accept.
_Ptr<int> f100(int a, int b);

_Ptr<int> f100(a, b)
     int a;
     int b; {
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Test error checking for invalid combinations of declaration specifiers.   //
// Incorrect code similar to this caused a crash in clang                    //
///////////////////////////////////////////////////////////////////////////////

void f101(void) {
  _Array_ptr<int> void a; // expected-error {{cannot combine with previous '_Array_ptr' declaration specifier}}
  int _Array_ptr<int> b;  // expected-error {{cannot combine with previous 'int' declaration specifier}}
  _Ptr<int> void c = 0;   // expected-error {{cannot combine with previous '_Ptr' declaration specifier}}
  int _Ptr<int> d;        // expected-error {{cannot combine with previous 'int' declaration specifier}}
}

///////////////////////////////////////////////////////////////////////////////
// Typechecking of function types with bounds in clang only supports         //
// function types with bounds expressions that refer to parameters declared  //
// in the function declarator or global varibles.  Other forms of            //
// dependently-typed function types are not supported.  They would require   //
// significant changes to C typechecking                                     //
//                                                                           //
// There are checks that enforce these restriction  to avoid ill-defined     //
// behavior on the part of the compiler.  Test the checks.                   //
///////////////////////////////////////////////////////////////////////////////

//
// Test that bounds in function types can only reference
// parameters in their parameter list or global variables
//

// Bounds for parameters reference a parameter in an enclosing parameter list.
void f300(int i, _Array_ptr<int>(*fnptr)(_Array_ptr<int> arg : count(i)));  // expected-error {{out-of-scope variable for bounds}}
void f301(int i, _Array_ptr<int>(*fnptr)(void) : count(i));                // expected-error {{out-of-scope variable for bounds}}
void f302(int i, _Ptr<_Array_ptr<int>(void) : count(i)> fnptr);             // expected-error {{out-of-scope variable for bounds}}

                                                                            // Bounds in function return type reference a parameter in the enclosing
                                                                            // parameter list.  This function is similar to what we would like to have for
                                                                            // bsearch and qsort.
_Ptr<int(_Array_ptr<void> : byte_count(i), _Array_ptr<void> : byte_count(i))>  // expected-error 2 {{use of undeclared identifier}}
f304(int i, _Ptr<int(_Array_ptr<void> : byte_count(i), _Array_ptr<void> : byte_count(i))> cmp); // expected-error  2 {{out-of-scope variable for bounds}}

                                                                                               // Bounds in a function type reference parameters or locals.
void f305(int i) {
  int j = i;
  _Ptr<_Array_ptr<int>(void) : count(i)> p = 0;    // expected-error {{out-of-scope variable for bounds}}
  _Ptr<_Array_ptr<int>(int k) : count(i)> q = 0; ; // expected-error {{out-of-scope variable for bounds}}
  _Ptr<_Array_ptr<int>(void) : count(j)> r = 0;    // expected-error {{out-of-scope variable for bounds}}

}

// Global variable bounds are OK.
int n;
_Ptr<int(_Array_ptr<void> : byte_count(n), _Array_ptr<void> : byte_count(n))>
fn306(_Ptr<int(_Array_ptr<void> : byte_count(n), _Array_ptr<void> : byte_count(n))> cmp);
