// Tests for checking that function declarations involving range bounds
// account for relative alignment information.
//
// RUN: %clang -cc1 -Wcheck-bounds-decls -verify %s

//
// Test declaration and definition of functions with a range bounds parameter with a rel_align clause.
//

typedef int intType1;
typedef int intType2;

struct S1 {
  int a;
  int b;
};

struct S2 {
  int a;
  int b;
};

typedef struct S1 T1;

// Pass: no rel_align clauses
void pass1(_Array_ptr<int> p
             : bounds(p, p + 1));

void pass1(_Array_ptr<int> p
             : bounds(p, p + 1)) {}

// Pass: same primitive type
void pass2(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align(char));

void pass2(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align(char)) {}

// Pass: aliases to the same type
void pass3(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align(intType1));

void pass3(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align(intType2)) {}

// Pass: same struct types
void pass4(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align(struct S1));

void pass4(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align(struct S1)) {}

// Pass: struct type and alias to struct type
void pass5(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align(struct S1));

void pass5(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align(T1)) {}

// Fail: rel_align and rel_align_value clauses
void fail1(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align(long)); // expected-note {{previous bounds declaration is here}}

void fail1(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align_value(4)) {} // expected-error {{function redeclaration has conflicting parameter bounds}}

// Fail: rel_align clause in definition but not declaration
void fail2(_Array_ptr<int> p
             : bounds(p, p + 1)); // expected-note {{previous bounds declaration is here}}

void fail2(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align(int)) {} // expected-error {{function redeclaration has conflicting parameter bounds}}

// Fail: different primitive types
void fail3(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align(int)); // expected-note {{previous bounds declaration is here}}

void fail3(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align(long)) {} // expected-error {{function redeclaration has conflicting parameter bounds}}

// Fail: primitive and struct types
void fail4(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align(short)); // expected-note {{previous bounds declaration is here}}

void fail4(_Array_ptr<int> p
             : bounds(p, p + 1) rel_align(struct S1)) {} // expected-error {{function redeclaration has conflicting parameter bounds}}

//
// Test declaration and definition of functions with a range bounds parameter with a rel_align_value clause.
//

const int val1 = 6;
const int val2 = 6;

// Pass: no rel_align_value clauses
void pass10(_Array_ptr<int> p
                  : bounds(p, p + 1));

void pass10(_Array_ptr<int> p
                  : bounds(p, p + 1)) {}

// Pass: same integer literals
void pass20(_Array_ptr<int> p
            : bounds(p, p + 1) rel_align_value(6));

void pass20(_Array_ptr<int> p
            : bounds(p, p + 1) rel_align_value(6)) {}

// Pass: same const int references
void pass30(_Array_ptr<int> p
            : bounds(p, p + 1) rel_align_value(val1));

void pass30(_Array_ptr<int> p
            : bounds(p, p + 1) rel_align_value(val1)) {}

// Fail: rel_align_value clause in declaration but not in definition
void fail10(_Array_ptr<int> p
            : bounds(p, p + 1) rel_align_value(val1)); // expected-note {{previous bounds declaration is here}}

void fail10(_Array_ptr<int> p
            : bounds(p, p + 1)) {} // expected-error {{function redeclaration has conflicting parameter bounds}}

// Fail: different integer literals
void fail20(_Array_ptr<int> p
            : bounds(p, p + 1) rel_align_value(6)); // expected-note {{previous bounds declaration is here}}

void fail20(_Array_ptr<int> p
            : bounds(p, p + 1) rel_align_value(7)) {} // expected-error {{function redeclaration has conflicting parameter bounds}}

// Fail: integer literal and const int reference
void fail30(_Array_ptr<int> p
            : bounds(p, p + 1) rel_align_value(6)); // expected-note {{previous bounds declaration is here}}

void fail30(_Array_ptr<int> p
            : bounds(p, p + 1) rel_align_value(val1)) {} // expected-error {{function redeclaration has conflicting parameter bounds}}

// Fail: different const int references
void fail40(_Array_ptr<int> p
            : bounds(p, p + 1) rel_align_value(val1)); // expected-note {{previous bounds declaration is here}}

void fail40(_Array_ptr<int> p
            : bounds(p, p + 1) rel_align_value(val2)) {} // expected-error {{function redeclaration has conflicting parameter bounds}}
