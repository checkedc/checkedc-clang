// Tests that implementation-specific definitions of NULL can be used
// in checked scopes.

// RUN: %clang_cc1 -verify -fcheckedc-extension -Wno-unused-value %s

//=========================================================================================
// Error messages for declarations of variables and members with unchecked types in checked scopes.
//=========================================================================================

extern _Checked void f1(int *p) { // expected-error {{parameter in a checked scope must have a checked type or a bounds-safe interface}}
}

extern _Checked void f2(int p[]) { // expected-error {{parameter in a checked scope must have a checked type or a bounds-safe interface}}
}

extern _Checked void f3(void(*pf)(void)) { // expected-error {{parameter in a checked scope must have a checked type or a bounds-safe interface}}
}

extern void f4(void) _Checked {
  int *p = 0;  // expected-error {{local variable in a checked scope must have a checked type}}
}

#pragma CHECKED_SCOPE ON
extern int *gp1; // expected-error {{global variable in a checked scope must have a checked type or a bounds-safe interface}}

struct S1 {
  int *f;       // expected-error {{member in a checked scope must have a checked type or a bounds-safe interface}}
};
#pragma CHECKED_SCOPE OFF

//=========================================================================================
// Error messages for uses of variables and members with unchecked types in checked scopes.
//=========================================================================================

extern void f10(int *p) _Checked { // expected-note {{parameter declared here}}
  p = 0; // expected-error {{parameter used in a checked scope must have a checked type or a bounds-safe interface}}
}

extern void f11(void) {
  int p[10];  // expected-note {{local variable declared here}}
  _Checked {
    p - 0; // expected-error {{local variable used in a checked scope must have a checked type}}
  }
}

extern int *gp2; // expected-note {{global variable declared here}}
extern void f12(void) {
  _Checked {
    gp2 = 0; // expected-error {{global variable used in a checked scope must have a checked type or a bounds-safe interface}}
  }
}

struct S2 {
  int *f;    // expected-note {{member declared here}}
};

extern void f13(void) {
  struct S2 tmp;
  _Checked{
    tmp.f = 0; // expected-error {{member used in a checked scope must have a checked type or a bounds-safe interface}}
  }
}

//=========================================================================================
// Test notes pointing out unchecked types that are problematic.
//=========================================================================================

typedef int *ty;

extern _Checked void f20(_Array_ptr<int *> p) { // expected-error {{parameter in a checked scope must have a pointer, array or function type that uses only checked types or parameter/return types with bounds-safe interfaces}} \
                                                // expected-note {{'int *' is not allowed in a checked scope}}
  p = 0;
}

extern _Checked void f21(_Array_ptr<ty> p) { // expected-error {{parameter in a checked scope must have a pointer, array or function type that uses only checked types or parameter/return types with bounds-safe interfaces}} \
                                             // expected-note {{'ty' (aka 'int *') is not allowed in a checked scope}}
  p = 0;
}


extern void f22(ty p) _Checked { // expected-note {{parameter declared here}}
  p = 0; // expected-error {{parameter used in a checked scope must have a checked type or a bounds-safe interface}} \
         // expected-note {{'ty' (aka 'int *') is not allowed in a checked scope}}
}

typedef int arrty[10];

// Make sure original (unadjusted) parameter array type is used in note.
extern void f23(arrty p) _Checked { // expected-note {{parameter declared here}}
  p = 0; // expected-error {{parameter used in a checked scope must have a checked type or a bounds-safe interface}} \
         // expected-note {{'arrty' (aka 'int [10]') is not allowed in a checked scope}}
}

// For function types that are adjusted, make sure the adjusted type is used 
// in note.  We use the adjust type so that it is clear that the type has been
// adjusted to be an unchecked type.
extern _Checked void f24(void(pf)(void)) { // expected-error {{parameter in a checked scope must have a checked type or a bounds-safe interface}} \
                                           // expected-note {{'void (*)(void)' is not allowed in a checked scope}}
}

extern void f25(void) {
  _Array_ptr<int[10]> p = 0;  // expected-note {{local variable declared here}}
  _Checked{
    p - 0; // expected-error {{local variable used in a checked scope must have a pointer, array or function type that uses only checked types or parameter/return types with bounds-safe interfaces}} \
                      // expected-note {{'int [10]' is not allowed in a checked scope}}
  }
}

#pragma CHECKED_SCOPE ON
extern ty gp3; // expected-error {{global variable in a checked scope must have a checked type or a bounds-safe interface}} \
               // expected-note {{'ty' (aka 'int *') is not allowed in a checked scope}}
#pragma CHECKED_SCOPE OFF

struct S3 {
  ty f;       // expected-note {{member declared here}}
};

extern _Checked void f26(struct S3 s) {
  s.f = 0;  // expected-error {{member used in a checked scope must have a checked type or a bounds-safe interface}} \
            // expected-note {{'ty' (aka 'int *') is not allowed in a checked scope}}
}

extern _Checked void f27(_Ptr<int(int *p, int count)> fp) // expected-error {{parameter in a checked scope must have a pointer, array or function type that uses only checked types or parameter/return types with bounds-safe interfaces}} \
                                                          // expected-note {{'int *' is not allowed in a checked scope}}
{ 
}

//=========================================================================================
//  Note for use of a bounds-safe interface that contains an unchecked type.
//=========================================================================================

extern _Checked void f30(int **p : itype(_Ptr<int *>)); // expected-error {{parameter in a checked scope must have a bounds-safe interface type that uses only checked types or parameter/return types with bounds-safe interfaces}} \
                                                        // expected-note {{'int *' is not allowed in a checked scope}}

extern _Checked void f30(int **p : itype(_Ptr<ty>)); // expected-error {{parameter in a checked scope must have a bounds-safe interface type that uses only checked types or parameter/return types with bounds-safe interfaces}} \
                                                     // expected-note {{'ty' (aka 'int *') is not allowed in a checked scope}}