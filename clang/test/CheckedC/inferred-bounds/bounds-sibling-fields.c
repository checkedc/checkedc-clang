// Tests for fields occuring in the declared bounds of sibling fields
// (fields declared in the same struct).
//
// RUN: %clang_cc1 -fdump-boundssiblingfields -verify -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s | FileCheck %s

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
  _Array_ptr<int> start;
  _Array_ptr<int> end;
  _Array_ptr<int> p : count(b->c->len + b->i);
  _Array_ptr<int> q : bounds(start, end);
  _Array_ptr<int> r : bounds(q, q + i);
};

void f1(struct A *a) {
  // CHECK-LABEL: In function: f1
  // CHECK: BoundsSiblingFields:
  // CHECK: A::b: { A::p }
  // CHECK: A::end: { A::q }
  // CHECK: A::i: { A::r }
  // CHECK: A::p: { A::p }
  // CHECK: A::q: { A::r }
  // CHECK: A::start: { A::q }
  // CHECK: B::c: { B::q }
  // CHECK: B::q: { B::q }
  // CHECK: C::len: { C::r }
  // CHECK: C::r: { C::r }
}

struct Node {
  struct Node *n;
  int len;
  _Array_ptr<int> p : byte_count(n->len);
};

void f2() {
  struct Node *node = 0;
  // CHECK-LABEL: In function: f2
  // CHECK: BoundsSiblingFields:
  // CHECK: Node::n: { Node::p }
  // CHECK: Node::p: { Node::p }
}

struct X;

struct Y {
  _Array_ptr<struct X> x;
  _Array_ptr<struct X> p : bounds(x, x);
};

struct X {
  struct Y *y;
  int len;
  int i;
  int j;
  _Array_ptr<int> q : bounds(y->p, y->p + y->x->len + i + j);
};

void f3(struct X x) {
  // CHECK-LABEL: In function: f3
  // CHECK: BoundsSiblingFields:
  // CHECK: X::i: { X::q }
  // CHECK: X::j: { X::q }
  // CHECK: X::y: { X::q }
  // CHECK: Y::x: { Y::p }
}

struct R {
  int len;
  _Array_ptr<int> q : count(len);
};

struct S {
  _Array_ptr<_Ptr<struct R>> r;
  _Array_ptr<int> p : count((*r)->len);
};

void f4(_Ptr<struct S> s[3][4]) {
  // CHECK-LABEL: In function: f4
  // CHECK: BoundsSiblingFields:
  // CHECK: R::len: { R::q }
  // CHECK: R::q: { R::q }
  // CHECK: S::p: { S::p }
  // CHECK: S::r: { S::p }
}

void f5(void) {
  ((struct A){ 0 }).i = 0; // expected-error {{inferred bounds for '((struct A){0}).r' are unknown after assignment}}
  // CHECK-LABEL: In function: f5
  // CHECK: BoundsSiblingFields:
  // CHECK: A::b: { A::p }
  // CHECK: A::end: { A::q }
  // CHECK: A::i: { A::r }
  // CHECK: A::p: { A::p }
  // CHECK: A::q: { A::r }
  // CHECK: A::start: { A::q }
  // CHECK: B::c: { B::q }
  // CHECK: B::q: { B::q }
  // CHECK: C::len: { C::r }
  // CHECK: C::r: { C::r }
}

struct X *getX(void) { return 0; }

void f6(void) {
  getX()->len = 0;
  // CHECK-LABEL: In function: f6
  // CHECK: BoundsSiblingFields:
  // CHECK: X::i: { X::q }
  // CHECK: X::j: { X::q }
  // CHECK: X::y: { X::q }
  // CHECK: Y::x: { Y::p }
}
