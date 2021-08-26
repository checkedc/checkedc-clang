// This file tests the compiler implementation of the support for
// bundled statements 
//
// The following line is for the LLVM test harness:
//
// RUN: %clang_cc1 -verify -verify-ignore-unexpected=note %s


void f1()
{
  _Bundled {
    unsigned int len;
    if (len) len++;                // expected-error {{a bundled block can contain only declarations and expression statements}}
    _Array_ptr<int> p : count(len) = 0;
  }
}


struct s                           // expected-error {{expected ';' after struct}}
_Bundled                           // expected-error {{expected identifier or '('}}
{
   unsigned n;
   _Array_ptr<int> p : count(n);
};

struct s                           // expected-error {{expected ';' after struct}}
_Checked _Bundled                  // expected-error {{expected identifier or '('}}
{
   unsigned n;
   _Array_ptr<int> p : count(n);
};

struct s                           // expected-error {{expected ';' after struct}}
_Checked _Bounds_only _Bundled     // expected-error {{expected identifier or '('}}
{
   unsigned n;
   _Array_ptr<int> p : count(n);
};


_Bundled void f2()                // expected-error {{expected identifier or '('}}
{
  {
    unsigned int len;
    _Array_ptr<int> p : count(len) = 0;
  }
}

_Checked _Bundled void f3()       // expected-error {{expected identifier or '('}}
{
  {
    unsigned int len;
    _Array_ptr<int> p : count(len) = 0;
  }
}

