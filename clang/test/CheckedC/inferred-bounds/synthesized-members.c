// Tests for synthesizing AbstractSets for member expressions whose target
// bounds use the value of a member expression being modified in an assignment.
//
// RUN: %clang_cc1 -fdump-synthesized-members -verify -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s | FileCheck %s

// expected-no-diagnostics

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
  x.y.z.len++;
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
  x.y.z = z;
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
  x.y = y;
  // CHECK: AbstractSets for member expressions:
  // CHECK: {
  // CHECK: +
  // CHECK:   .
  // CHECK:     DeclRefExpr {{.*}} 'x'
  // CHECK:     FieldDecl {{.*}} p
  // CHECK:   IntegerLiteral {{.*}} 0
  // CHECK: }
};

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
  a->b->c->len += 1;
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
  (((a + 0)->b + 0)->c + 0)->len--;
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
  (*((*((*a).b)).c)).len = 0;
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
  a[0].b[0].c[0].len = 0;
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
  a[1 - 1].b[2 - 2].c[3 - 3].len = 0;
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
  a->start = 0;
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
