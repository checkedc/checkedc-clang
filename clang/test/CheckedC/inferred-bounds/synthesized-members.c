// Tests for synthesizing AbstractSets for member expressions whose target
// bounds use the value of a member expression being modified in an assignment.
//
// RUN: %clang_cc1 -fdump-synthesized-members -verify -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s | FileCheck %s

struct Z {
  int len;
  _Array_ptr<int> r : count(len);
};

struct Y {
  struct Z z;
  _Array_ptr<int> q : count(z.len);
};

struct X {
  struct Y y;
  _Array_ptr<int> p : count(y.z.len);
};

void f1(struct X x, struct Y y, struct Z z) {
  // Member expressions whose bounds use x.y.z.len: { x.p, x.y.q, x.y.z.r }
  x.y.z.len++; // expected-error {{inferred bounds for 'x.p' are unknown after increment}} \
               // expected-error {{inferred bounds for 'x.y.q' are unknown after increment}} \
               // expected-error {{inferred bounds for 'x.y.z.r' are unknown after increment}}
  // CHECK:  AbstractSets for member expressions:
  // CHECK:  {
  // CHECK:  +
  // CHECK:    .
  // CHECK:      DeclRefExpr {{.*}} 'x'
  // CHECK:      FieldDecl {{.*}} p
  // CHECK:    IntegerLiteral {{.*}} 0
  // CHECK:  +
  // CHECK:    .
  // CHECK:      .
  // CHECK:        DeclRefExpr {{.*}} 'x'
  // CHECK:        FieldDecl {{.*}} y
  // CHECK:      FieldDecl {{.*}} q
  // CHECK:    IntegerLiteral {{.*}} 0
  // CHECK:  +
  // CHECK:    .
  // CHECK:      .
  // CHECK:        .
  // CHECK:          DeclRefExpr {{.*}} 'x'
  // CHECK:          FieldDecl {{.*}} y
  // CHECK:        FieldDecl {{.*}} z
  // CHECK:      FieldDecl {{.*}} r
  // CHECK:    IntegerLiteral {{.*}} 0
  // CHECK:  }

  // Member expressions whose bounds use x.y.z: { x.p, x.y.q }
  x.y.z = z; // expected-error {{inferred bounds for 'x.p' are unknown after assignment}} \
             // expected-error {{inferred bounds for 'x.y.q' are unknown after assignment}}
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   .
  // CHECK:     DeclRefExpr {{.*}} 'x'
  // CHECK:     FieldDecl {{.*}} p
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   .
  // CHECK:     .
  // CHECK:       DeclRefExpr {{.*}} 'x'
  // CHECK:       FieldDecl {{.*}} y
  // CHECK:     FieldDecl {{.*}} q
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: }

  // Member expressions whose bounds use x.y: { x.p }
  x.y = y; // expected-error {{inferred bounds for 'x.p' are unknown after assignment}}
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   .
  // CHECK:     DeclRefExpr {{.*}} 'x'
  // CHECK:     FieldDecl {{.*}} p
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: }
}

struct C {
  int len;
  _Array_ptr<int> r : count(len);
};

struct B {
  int i;
  int j;
  struct C *c;
  _Array_ptr<int> q : count(c->len);
};

struct A {
  struct B *b;
  int i;
  int j;
  _Array_ptr<int> start : bounds(start, end);
  _Array_ptr<int> end;
  _Array_ptr<int> p : count(b->c->len + b->i);
  _Array_ptr<int> q : bounds(start, end);
  _Array_ptr<int> r : bounds(q, q + i);
};

void f2(_Array_ptr<struct A> a : count(10)) {
  // Member expressions whose bounds depend on a->b->c->len: { a->p, a->b->q, a->b->c->r }
  a->b->c->len += 1; // expected-error {{inferred bounds for 'a->p' are unknown after assignment}} \
                     // expected-error {{inferred bounds for 'a->b->q' are unknown after assignment}} \
                     // expected-error {{inferred bounds for 'a->b->c->r' are unknown after assignment}}
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 'a'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} p
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         ->
  // CHECK:           +
  // CHECK:             LValueToRValue
  // CHECK:               DeclRefExpr {{.*}} 'a'
  // CHECK:             IntegerLiteral {{.*}} 0
  // CHECK:           FieldDecl {{.*}} b
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} q
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         ->
  // CHECK:           +
  // CHECK:             LValueToRValue
  // CHECK:               ->
  // CHECK:                 +
  // CHECK:                   LValueToRValue
  // CHECK:                     DeclRefExpr {{.*}} 'a'
  // CHECK:                   IntegerLiteral {{.*}} 0
  // CHECK:                 FieldDecl {{.*}} b
  // CHECK:             IntegerLiteral {{.*}} 0
  // CHECK:           FieldDecl {{.*}} c
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} r
  // CHECK:   IntegerLiteral {{.*}} 0

  // Member expressions whose bounds depend on (((a + 0)->b + 0)->c + 0)->len: { a->p, a->b->q, a->b->c->r }
  (((a + 0)->b + 0)->c + 0)->len--; // expected-error {{inferred bounds for 'a->p' are unknown after decrement}} \
                                    // expected-error {{inferred bounds for 'a->b->q' are unknown after decrement}} \
                                    // expected-error {{inferred bounds for 'a->b->c->r' are unknown after decrement}}
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 'a'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} p
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         ->
  // CHECK:           +
  // CHECK:             LValueToRValue
  // CHECK:               DeclRefExpr {{.*}} 'a'
  // CHECK:             IntegerLiteral {{.*}} 0
  // CHECK:           FieldDecl {{.*}} b
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} q
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         ->
  // CHECK:           +
  // CHECK:             LValueToRValue
  // CHECK:               ->
  // CHECK:                 +
  // CHECK:                   LValueToRValue
  // CHECK:                     DeclRefExpr {{.*}} 'a'
  // CHECK:                   IntegerLiteral {{.*}} 0
  // CHECK:                 FieldDecl {{.*}} b
  // CHECK:             IntegerLiteral {{.*}} 0
  // CHECK:           FieldDecl {{.*}} c
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} r
  // CHECK:   IntegerLiteral {{.*}} 0

  // Member expressions whose bounds depend on (*((*((*a).b)).c)).len: { a->p, a->b->q, a->b->c->r }
  (*((*((*a).b)).c)).len = 0; // expected-error {{inferred bounds for 'a->p' are unknown after assignment}} \
                              // expected-error {{inferred bounds for 'a->b->q' are unknown after assignment}} \
                              // expected-error {{inferred bounds for 'a->b->c->r' are unknown after assignment}}
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 'a'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} p
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         ->
  // CHECK:           +
  // CHECK:             LValueToRValue
  // CHECK:               DeclRefExpr {{.*}} 'a'
  // CHECK:             IntegerLiteral {{.*}} 0
  // CHECK:           FieldDecl {{.*}} b
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} q
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         ->
  // CHECK:           +
  // CHECK:             LValueToRValue
  // CHECK:               ->
  // CHECK:                 +
  // CHECK:                   LValueToRValue
  // CHECK:                     DeclRefExpr {{.*}} 'a'
  // CHECK:                   IntegerLiteral {{.*}} 0
  // CHECK:                 FieldDecl {{.*}} b
  // CHECK:             IntegerLiteral {{.*}} 0
  // CHECK:           FieldDecl {{.*}} c
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} r
  // CHECK:   IntegerLiteral {{.*}} 0

  // Member expressions whose bounds depend on a[0].b[0].c[0].len: { a->p, a->b->q, a->b->c->r }
  a[0].b[0].c[0].len = 0; // expected-error {{inferred bounds for 'a->p' are unknown after assignment}} \
                          // expected-error {{inferred bounds for 'a->b->q' are unknown after assignment}} \
                          // expected-error {{inferred bounds for 'a->b->c->r' are unknown after assignment}}
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 'a'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} p
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         ->
  // CHECK:           +
  // CHECK:             LValueToRValue
  // CHECK:               DeclRefExpr {{.*}} 'a'
  // CHECK:             IntegerLiteral {{.*}} 0
  // CHECK:           FieldDecl {{.*}} b
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} q
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         ->
  // CHECK:           +
  // CHECK:             LValueToRValue
  // CHECK:               ->
  // CHECK:                 +
  // CHECK:                   LValueToRValue
  // CHECK:                     DeclRefExpr {{.*}} 'a'
  // CHECK:                   IntegerLiteral {{.*}} 0
  // CHECK:                 FieldDecl {{.*}} b
  // CHECK:             IntegerLiteral {{.*}} 0
  // CHECK:           FieldDecl {{.*}} c
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} r
  // CHECK:   IntegerLiteral {{.*}} 0

  // Member expressions whose bounds depend on a[1 - 1].b[2 - 2].c[3 - 3].len: { a->p, a->b->q, a->b->c->r }
  a[1 - 1].b[2 - 2].c[3 - 3].len = 0; // expected-error {{inferred bounds for 'a->p' are unknown after assignment}} \
                                      // expected-error {{inferred bounds for 'a->b->q' are unknown after assignment}} \
                                      // expected-error {{inferred bounds for 'a->b->c->r' are unknown after assignment}}
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 'a'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} p
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         ->
  // CHECK:           +
  // CHECK:             LValueToRValue
  // CHECK:               DeclRefExpr {{.*}} 'a'
  // CHECK:             IntegerLiteral {{.*}} 0
  // CHECK:           FieldDecl {{.*}} b
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} q
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         ->
  // CHECK:           +
  // CHECK:             LValueToRValue
  // CHECK:               ->
  // CHECK:                 +
  // CHECK:                   LValueToRValue
  // CHECK:                     DeclRefExpr {{.*}} 'a'
  // CHECK:                   IntegerLiteral {{.*}} 0
  // CHECK:                 FieldDecl {{.*}} b
  // CHECK:             IntegerLiteral {{.*}} 0
  // CHECK:           FieldDecl {{.*}} c
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} r
  // CHECK:   IntegerLiteral {{.*}} 0
}

void f3(struct A *a, struct B *b) {
  // Member expressions whose bounds depend on a->start: { a->q, a->start }
  a->start = 0; // expected-error {{inferred bounds for 'a->q' are unknown after assignment}}
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 'a'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} q
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 'a'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} start
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: }

  // Member expressions whose bounds depend on b->q: { b->q }
  b->q = 0;
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 'b'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} q
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: }
}

struct S {
  _Array_ptr<int> f : bounds(*arr, *arr + *len);
  _Array_ptr<int> g : count(*len);
  _Array_ptr<int> *arr;
  _Array_ptr<int> len : count(10);
};

struct R {
  _Array_ptr<int> a : count(*s->len);
  _Array_ptr<struct S> s : count(10);
};

void f4(struct R *r, struct S *s) {
  // Member expressions whose bounds depend on s->arr: { s->f }
  s->arr = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}}
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 's'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} f
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: }

  // Member expressions whose bounds depend on s->len: { s->f, s->g }
  s->len = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
              // expected-error {{inferred bounds for 's->g' are unknown after assignment}}
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 's'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} f
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 's'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} g
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: }

  // Member expressions whose bounds depend on s->arr[0]: { s->f }
  s->arr[0] = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}}
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 's'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} f
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: }

  // Member expressions whose bounds depend on *(s->arr + 1) : { }
  *(s->arr + 1) = 0;
  // CHECK: AbstractSets for member expressions:
  // CHECK: { }

  // Member expressions whose bounds depend on *s->len: { s->f, s->g }
  *s->len = 0; // expected-error {{inferred bounds for 's->f' are unknown after assignment}} \
               // expected-error {{inferred bounds for 's->g' are unknown after assignment}}
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 's'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} f
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 's'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} g
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: }

  // Member expressions whose bounds depend on *r->s->len: { r->a, r->s->f, r->s->g }
  *r->s->len = 0; // expected-error {{inferred bounds for 'r->a' are unknown after assignment}} \
                  // expected-error {{inferred bounds for 'r->s->f' are unknown after assignment}} \
                  // expected-error {{inferred bounds for 'r->s->g' are unknown after assignment}}
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         DeclRefExpr {{.*}} 'r'
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} a
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         ->
  // CHECK:           +
  // CHECK:             LValueToRValue
  // CHECK:               DeclRefExpr {{.*}} 'r'
  // CHECK:             IntegerLiteral {{.*}} 0
  // CHECK:           FieldDecl {{.*}} s
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} f
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: +
  // CHECK:   ->
  // CHECK:     +
  // CHECK:       LValueToRValue
  // CHECK:         ->
  // CHECK:           +
  // CHECK:             LValueToRValue
  // CHECK:               DeclRefExpr {{.*}} 'r'
  // CHECK:             IntegerLiteral {{.*}} 0
  // CHECK:           FieldDecl {{.*}} s
  // CHECK:       IntegerLiteral {{.*}} 0
  // CHECK:     FieldDecl {{.*}} g
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: }
}
