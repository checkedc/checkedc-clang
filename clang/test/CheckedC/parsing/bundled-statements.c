// This file tests the compiler implementation of the support for
// bundled statements 
//
// The following line is for the LLVM test harness:
//
// RUN: %clang_cc1 -verify -verify-ignore-unexpected=note %s


// Tests for invalid bundled statements.

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

void f4(int flag1, int flag2)
_Unchecked{
  _Array_ptr<int> p : count(2) = 0;
  int val = 5;
  int val1 _Checked[3];
  _Array_ptr<int> q : count(1) = &val;
  L1:
  _Bundled {
    p = val1;
    p++;                                  // expected-warning {{cannot prove declared bounds for 'p' are valid after increment}}
    _Bundled {                            // expected-error {{a bundled block can contain only declarations and expression statements}}
      p = flag1 ? q : flag2 ? q : val1;   // expected-error {{inferred bounds for 'p' are unknown after assignment}}
      *(p+1) = 4;                         // expected-error {{expression has unknown bounds}}
    }
  }
}

void f5()
_Unchecked{
  _Array_ptr<int> p : count(2) = 0;
  int val _Checked[3];
  _Bundled {
    p = val;
    L1: p++;                             // expected-error {{a bundled block can contain only declarations and expression statements}} \
                                         // expected-warning {{cannot prove declared bounds for 'p' are valid after increment}}
  }
}

void f6()
{
  _Bundle {                              // expected-error {{use of undeclared identifier '_Bundle'}}
    unsigned int len;
    _Array_ptr<int> p : count(len) = 0;
  }
}

void f7()
{
  _bundle {                              // expected-error {{use of undeclared identifier '_bundle'}}
    unsigned int len;
    _Array_ptr<int> p : count(len) = 0;
  }
}

void f8()
{
  _bundled {                             // expected-error {{use of undeclared identifier '_bundled'}}
    unsigned int len;
    _Array_ptr<int> p : count(len) = 0;
  }
}

void f9()
_Bundled {                               // expected-error {{function body cannot be a bundled block}}
    unsigned int len;
    _Array_ptr<int> p : count(len) = 0;
}

