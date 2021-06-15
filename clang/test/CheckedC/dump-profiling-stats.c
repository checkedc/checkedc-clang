// Tests for dumping Checked C profiling information.
//
// RUN: %clang_cc1 -print-stats -verify -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s | grep '' %s | FileCheck %s --dump-input=always

struct B {
  int len;
};

struct A {
  struct B *b;
  _Array_ptr<int> f : count(b->len);
  _Array_ptr<int> g : count(b[1].len);
  _Array_ptr<int> h : count(1);
};

// Test printing profiling counters for synthesizing member expressions
// and AbstractSets.
void synthesize_members(struct A *a) {
  // While synthesizing members for the following assignment, MemberExprs
  // will be created for a->f and a->g. An AbstractSet will be created
  // for a->f.
  a->b->len = 0; // expected-error {{inferred bounds for 'a->f' are unknown after assignment}}
}

// CHECK: *** Checked C Stats:
// CHECK: 2 MemberExprs created while synthesizing members.
// CHECK: 1 AbstractSets created while synthesizing members.
