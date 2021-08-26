// Tests for available facts (count) presence of where clauses as 
// expected.
//
// RUN: %clang_cc1 -fdump-available-facts -verify \
// RUN: -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s \
// RUN: | FileCheck %s
// expected-no-diagnostics

void basic_block(int x) {
  x = 5;
  _Where x > 3;
  _Where x > 4;
}
// CHECK-LABEL: basic_block
//
// CHECK: Block: B1; Pred: B2; Succ: B0
// CHECK: Gen [B1, AllSucc]:
// CHECK-DAG: x > 3
// CHECK-DAG: x > 4
// CHECK: Kill [B1, AllSucc]: x
// CHECK: In [B1]: {}
// CHECK: Out [B1, AllSucc]:
// CHECK-DAG: x > 3
// CHECK-DAG: x > 4

void one_kill(int x) {
  x = 5;
  _Where x > 3;
  x = 3;
  _Where x > 4;
}
// CHECK-LABEL: one_kill
//
// CHECK: Block: B1; Pred: B2; Succ: B0
// CHECK: Gen [B1, AllSucc]:
// CHECK:   x > 4
// CHECK: Kill [B1, AllSucc]: x
// CHECK: In [B1]: {}
// CHECK: Out [B1, AllSucc]:
// CHECK:   x > 4

void if_a_fact(int x, _Array_ptr<int> a) {
  if (x > 0) {
    _Where a : count(3);
  }
  x = 3;
}
// CHECK-LABEL: if_a_fact
//
// CHECK: Block: B3; Pred: B4; Succ: B2, B1
// CHECK: Gen [B3, AllSucc]: {}
// CHECK: Kill [B3, AllSucc]: {}
// CHECK: In [B3]: {}
// CHECK: Out [B3, AllSucc]: {}
// CHECK: Gen [B3 -> B2]:
// CHECK:   x > 0
// CHECK: Out [B3 -> B2]:
// CHECK:   x > 0
//
// CHECK: Block: B2; Pred: B3; Succ: B1
// CHECK: Gen [B2, AllSucc]:
//   a: bounds(a, a + 3)
// CHECK: Kill [B2, AllSucc]: a
// CHECK: In [B2]:
// CHECK:  x > 0
// CHECK: Out [B2, AllSucc]:
// CHECK-DAG: a: bounds(a, a + 3)
// CHECK-DAG: x > 0
//
// CHECK: Block: B1; Pred: B2, B3; Succ: B0
// CHECK: Gen [B1, AllSucc]: {}
// CHECK: Kill [B1, AllSucc]: x, 
// CHECK: In [B1]: {}
// CHECK: Out [B1, AllSucc]: {}

void if_then_else_differ_bounds(int x, _Array_ptr<int> a) {
  if (x > 0) {
    _Where a : count(3);
  } else {
    _Where a : count(5);
  }
  x = 1;
}
// CHECK-LABEL: if_then_else_differ_bounds
//
// CHECK: Block: B4; Pred: B5; Succ: B3, B2
// CHECK: Gen [B4, AllSucc]: {}
// CHECK: Kill [B4, AllSucc]: {}
// CHECK: In [B4]: {}
// CHECK: Out [B4, AllSucc]: {}
// CHECK: Gen [B4 -> B3]:
// CHECK:   x > 0
// CHECK: Out [B4 -> B3]:
// CHECK:   x > 0
// CHECK: Gen [B4 -> B2]:
// CHECK:   x <= 0
// CHECK: Out [B4 -> B2]:
// CHECK:   x <= 0
//
// CHECK: Block: B3; Pred: B4; Succ: B1
// CHECK: Gen [B3, AllSucc]:
// CHECK:   a: bounds(a, a + 3)
// CHECK: Kill [B3, AllSucc]: a
// CHECK: In [B3]:
// CHECK:   x > 0
// CHECK: Out [B3, AllSucc]:
// CHECK-DAG: a: bounds(a, a + 3)
// CHECK-DAG: x > 0
// CHECK: Gen [B3 -> B1]: {}
// CHECK: Out [B3 -> B1]:
// CHECK:   x > 0
//
// CHECK: Block: B2; Pred: B4; Succ: B1
// CHECK: Gen [B2, AllSucc]:
// CHECK:   a: bounds(a, a + 5)
// CHECK: Kill [B2, AllSucc]: a
// CHECK: In [B2]:
// CHECK:   x <= 0
// CHECK: Out [B2, AllSucc]:
// CHECK-DAG: a: bounds(a, a + 5)
// CHECK-DAG: x <= 0
// CHECK: Gen [B2 -> B1]: {}
// CHECK: Out [B2 -> B1]:
// CHECK:   x <= 0
//
// CHECK: Block: B1; Pred: B2, B3; Succ: B0
// CHECK: Gen [B1, AllSucc]: {}
// CHECK: Kill [B1, AllSucc]: x, 
// CHECK: In [B1]: {}
// CHECK: Out [B1, AllSucc]: {}

void if_then_else_eq_bound(int x, _Array_ptr<int> a) {
  if (x > 0) {
    _Where a : count(6);
  } else {
    _Where a : count(6);
  }
  x = 1;
}
// CHECK-LABEL: if_then_else_eq_bound
//
// CHECK: Block: B4; Pred: B5; Succ: B3, B2
// CHECK: Gen [B4, AllSucc]: {}
// CHECK: Kill [B4, AllSucc]: {}
// CHECK: In [B4]: {}
// CHECK: Out [B4, AllSucc]: {}
// CHECK: Gen [B4 -> B3]:
// CHECK:   x > 0
// CHECK: Out [B4 -> B3]:
// CHECK:   x > 0
// CHECK: Gen [B4 -> B2]:
// CHECK:   x <= 0
// CHECK: Out [B4 -> B2]:
// CHECK:   x <= 0
//
// CHECK: Block: B3; Pred: B4; Succ: B1
// CHECK: Gen [B3, AllSucc]:
// CHECK:   a: bounds(a, a + 6)
// CHECK: Kill [B3, AllSucc]: a
// CHECK: In [B3]:
// CHECK:   x > 0
// CHECK: Out [B3, AllSucc]:
// CHECK-DAG: a: bounds(a, a + 6)
// CHECK-DAG: x > 0
// CHECK: Gen [B3 -> B1]: {}
// CHECK: Out [B3 -> B1]:
// CHECK:   x > 0
//
// CHECK: Block: B2; Pred: B4; Succ: B1
// CHECK: Gen [B2, AllSucc]:
// CHECK:   a: bounds(a, a + 6)
// CHECK: Kill [B2, AllSucc]: a
// CHECK: In [B2]:
// CHECK:   x <= 0
// CHECK: Out [B2, AllSucc]:
// CHECK-DAG: a: bounds(a, a + 6)
// CHECK-DAG: x <= 0
// CHECK: Gen [B2 -> B1]: {}
// CHECK: Out [B2 -> B1]:
// CHECK:   x <= 0
//
// CHECK: Block: B1; Pred: B2, B3; Succ: B0
// CHECK: Gen [B1, AllSucc]: {}
// CHECK: Kill [B1, AllSucc]: x, 
// CHECK: In [B1]:
// CHECK:   a: bounds(a, a + 6)
// CHECK: Out [B1, AllSucc]:
// CHECK:   a: bounds(a, a + 6)

void para_where(int x _Where x > 3) {
  x = 1;
}
// CHECK-LABEL: para_where
//
// CHECK: Block: B2; Pred: ; Succ: B1
// CHECK: Gen [B2, AllSucc]:
// CHECK:   x > 3
// CHECK: Kill [B2, AllSucc]: x
// CHECK: In [B2]: {}
// CHECK: Out [B2, AllSucc]:
// CHECK:   x > 3
// CHECK: Gen [B2 -> B1]: {}
// CHECK: Out [B2 -> B1]: {}
//
// CHECK: Block: B1; Pred: B2; Succ: B0
// CHECK: Gen [B1, AllSucc]: {}
// CHECK: Kill [B1, AllSucc]: x
// CHECK: In [B1]:
// CHECK:   x > 3
// CHECK: Out [B1, AllSucc]: {}

void switch_case_one(int x, int y) {
  switch (x) {
  case 1 : 
    y = 11;
    break;
  case 2:
    y = 12;
    break;
  default:
    y = 20;
    break;
  }
  y = 0;
}
// CHECK-LABEL: switch_case_one
//
// CHECK: B5; Pred: B2; Succ: B1
// CHECK: Gen [B5, AllSucc]: {}
// CHECK: Kill [B5, AllSucc]: y
// CHECK: In [B5]:
// CHECK-DAG:   x == 1
// CHECK: Out [B5, AllSucc]:
// CHECK-DAG:   x == 1
// CHECK: Gen [B5 -> B1]: {}
// CHECK: Out [B5 -> B1]:
// CHECK-DAG:   x == 1
//
// CHECK: B4; Pred: B2; Succ: B1
// CHECK: Gen [B4, AllSucc]: {}
// CHECK: Kill [B4, AllSucc]: y
// CHECK: In [B4]:
// CHECK-DAG:   x == 2
// CHECK: Out [B4, AllSucc]:
// CHECK-DAG:   x == 2
// CHECK: Gen [B4 -> B1]: {}
// CHECK: Out [B4 -> B1]:
// CHECK-DAG:   x == 2
//
// CHECK: B3; Pred: B2; Succ: B1
// TODO: it would be {x != 1, x != 2]
// CHECK: Gen [B3, AllSucc]: {}
// CHECK: Kill [B3, AllSucc]: y
// CHECK: In [B3]: {}
// CHECK: Out [B3, AllSucc]: {}
// CHECK: Gen [B3 -> B1]: {}
// CHECK: Out [B3 -> B1]:
//
// CHECK: B2; Pred: B6; Succ: B4, B5, B3
// CHECK: Gen [B2, AllSucc]: {}
// CHECK: Kill [B2, AllSucc]: {}
// CHECK: In [B2]: {}
// CHECK: Out [B2, AllSucc]: {}
// CHECK: Gen [B2 -> B4]:
// CHECK-DAG:   x == 2
// CHECK: Out [B2 -> B4]:
// CHECK-DAG:   x == 2
// CHECK: Gen [B2 -> B5]:
// CHECK-DAG:   x == 1
// CHECK: Out [B2 -> B5]:
// CHECK: Gen [B2 -> B3]: {}
// CHECK: Out [B2 -> B3]: {}
//
// CHECK: B1; Pred: B3, B4, B5; Succ: B0
// CHECK: Gen [B1, AllSucc]: {}
// CHECK: Kill [B1, AllSucc]: y
// CHECK: In [B1]: {}
// CHECK: Out [B1, AllSucc]: {}


void short_circuited_and(int x, int y) {
  int z = 0;
  if (x == 1 && y == 2)
    z = 3;
  else if (x == 1)
    z = 4;
}
// CHECK-LABEL: short_circuited_and
//
// CHECK: Block: B6; Pred: B7; Succ: B5, B3
// CHECK: Gen [B6, AllSucc]: {}
// CHECK: Kill [B6, AllSucc]: z
// CHECK: In [B6]: {}
// CHECK: Out [B6, AllSucc]: {}
// CHECK: Gen [B6 -> B5]:
// CHECK-DAG:   x == 1
// CHECK: Out [B6 -> B5]:
// CHECK-DAG:   x == 1
// CHECK: Gen [B6 -> B3]:
// CHECK-DAG:   x != 1
// CHECK: Out [B6 -> B3]:
// CHECK-DAG:   x != 1
//
// CHECK: Block: B5; Pred: B6; Succ: B4, B3
// CHECK: Gen [B5, AllSucc]: {}
// CHECK: Kill [B5, AllSucc]: {}
// CHECK: In [B5]:
// CHECK-DAG:   x == 1
// CHECK: Out [B5, AllSucc]:
// CHECK-DAG:   x == 1
// CHECK: Gen [B5 -> B4]:
// CHECK-DAG:   y == 2
// CHECK: Out [B5 -> B4]:
// CHECK-DAG:   y == 2
// CHECK-DAG:   x == 1
// CHECK: Gen [B5 -> B3]:
// CHECK-DAG:   y != 2
// CHECK: Out [B5 -> B3]:
// CHECK-DAG:   y != 2
// CHECK-DAG:   x == 1
//
// CHECK: Block: B4; Pred: B5; Succ: B1
// CHECK: Gen [B4, AllSucc]: {}
// CHECK: Kill [B4, AllSucc]: z
// CHECK: In [B4]:
// CHECK-DAG:   y == 2
// CHECK-DAG:   x == 1
// CHECK: Out [B4, AllSucc]:
// CHECK-DAG:   y == 2
// CHECK-DAG:   x == 1
// CHECK: Gen [B4 -> B1]: {}
// CHECK: Out [B4 -> B1]:
// CHECK-DAG:   y == 2
// CHECK-DAG:   x == 1
//
// CHECK: B3; Pred: B5, B6; Succ: B2, B1
// CHECK: Gen [B3, AllSucc]: {}
// CHECK: Kill [B3, AllSucc]: {}
// CHECK: In [B3]: {}
// CHECK: Out [B3, AllSucc]: {}
// CHECK: Gen [B3 -> B2]:
// CHECK-DAG:   x == 1
// CHECK: Out [B3 -> B2]:
// CHECK-DAG:   x == 1
// CHECK: Gen [B3 -> B1]:
// CHECK-DAG:   x != 1
// CHECK: Out [B3 -> B1]:
// CHECK-DAG:   x != 1
//
// CHECK: B2; Pred: B3; Succ: B1
// CHECK: Gen [B2, AllSucc]: {}
// CHECK: Kill [B2, AllSucc]: z
// CHECK: In [B2]:
// CHECK-DAG:   x == 1
// CHECK: Out [B2, AllSucc]:
// CHECK-DAG:   x == 1
// CHECK: Gen [B2 -> B1]: {}
// CHECK: Out [B2 -> B1]:
// CHECK-DAG:   x == 1

void where_kills_where(_Array_ptr<int> a _Where a : count(2)) {
  _Where a : count(3);
}
// CHECK-LABEL: Function: where_kills_where
//
// CHECK: Block: B2; Pred: ; Succ: B1
// CHECK: Gen [B2, AllSucc]:
// CHECK:   a: bounds(a, a + 2)
// CHECK: Kill [B2, AllSucc]: a
// CHECK: In [B2]: {}
// CHECK: Out [B2, AllSucc]:
// CHECK:   a: bounds(a, a + 2)
// CHECK: Gen [B2 -> B1]: {}
// CHECK: Out [B2 -> B1]: {}
//
// CHECK: Block: B1; Pred: B2; Succ: B0
// CHECK: Gen [B1, AllSucc]:
// CHECK:   a: bounds(a, a + 3)
// CHECK: Kill [B1, AllSucc]: a,
// CHECK: In [B1]:
// CHECK:   a: bounds(a, a + 2)
// CHECK: Out [B1, AllSucc]:
// CHECK:   a: bounds(a, a + 3)

void where_kills_type(_Array_ptr<int> a : count(2)) {
  _Where a : count(3);
}
// CHECK-LABEL: Function: where_kills_type
//
// CHECK: Block: B2; Pred: ; Succ: B1
// CHECK: Gen [B2, AllSucc]:
// CHECK:   a: bounds(a, a + 2)
// CHECK: Kill [B2, AllSucc]: a
// CHECK: In [B2]: {}
// CHECK: Out [B2, AllSucc]:
// CHECK:   a: bounds(a, a + 2)
// CHECK: Gen [B2 -> B1]: {}
// CHECK: Out [B2 -> B1]: {}
//
// CHECK: Block: B1; Pred: B2; Succ: B0
// CHECK: Gen [B1, AllSucc]:
// CHECK:   a: bounds(a, a + 3)
// CHECK: Kill [B1, AllSucc]: a,
// CHECK: In [B1]:
// CHECK:   a: bounds(a, a + 2)
// CHECK: Out [B1, AllSucc]:
// CHECK:   a: bounds(a, a + 3)

void the_same_redeclaration_after_where(int x, _Array_ptr<int> a _Where a : count(2)) {
  if (x) {
    _Where a : count(3);
  } else {
    _Where a : count(3);
  }
  x = 3;
}
// CHECK-LABEL: Function: the_same_redeclaration_after_where
//
// CHECK: Block: B4; Pred: B5; Succ: B3, B2
// CHECK: Gen [B4, AllSucc]: {}
// CHECK: Kill [B4, AllSucc]: {}
// CHECK: In [B4]:
// CHECK:   a: bounds(a, a + 2)
// CHECK: Out [B4, AllSucc]:
// CHECK:   a: bounds(a, a + 2)
// CHECK: Gen [B4 -> B3]:
// CHECK:   x != 0
// CHECK: Out [B4 -> B3]:
// CHECK-DAG:   x != 0
// CHECK-DAG:   a: bounds(a, a + 2)
// CHECK: Gen [B4 -> B2]:
// CHECK:   x == 0
// CHECK: Out [B4 -> B2]:
// CHECK-DAG:   x == 0
// CHECK-DAG:   a: bounds(a, a + 2)
//
// CHECK: Block: B1; Pred: B2, B3; Succ: B0
// CHECK: Gen [B1, AllSucc]: {}
// CHECK: Kill [B1, AllSucc]: x
// CHECK: In [B1]:
// CHECK:   a: bounds(a, a + 3)
// CHECK: Out [B1, AllSucc]:
// CHECK:   a: bounds(a, a + 3)

void the_same_redeclaration_after_type(int x, _Array_ptr<int> a : count(2)) {
  if (x) {
    _Where a : count(3);
  } else {
    _Where a : count(3);
  }
  x = 3;
}
// CHECK-LABEL: Function: the_same_redeclaration_after_type
//
// CHECK: Block: B4; Pred: B5; Succ: B3, B2
// CHECK: Gen [B4, AllSucc]: {}
// CHECK: Kill [B4, AllSucc]: {}
// CHECK: In [B4]:
// CHECK:   a: bounds(a, a + 2)
// CHECK: Out [B4, AllSucc]:
// CHECK:   a: bounds(a, a + 2)
// CHECK: Gen [B4 -> B3]:
// CHECK:   x != 0
// CHECK: Out [B4 -> B3]:
// CHECK-DAG:   x != 0
// CHECK-DAG:   a: bounds(a, a + 2)
// CHECK: Gen [B4 -> B2]:
// CHECK:   x == 0
// CHECK: Out [B4 -> B2]:
// CHECK-DAG:   x == 0
// CHECK-DAG:   a: bounds(a, a + 2)
//
// CHECK: Block: B1; Pred: B2, B3; Succ: B0
// CHECK: Gen [B1, AllSucc]: {}
// CHECK: Kill [B1, AllSucc]: x
// CHECK: In [B1]:
// CHECK:   a: bounds(a, a + 3)
// CHECK: Out [B1, AllSucc]:
// CHECK:   a: bounds(a, a + 3)

void different_redeclarations_after_where(int x, _Array_ptr<int> a _Where a : count(2)) {
  if (x) {
    _Where a : count(3);
  } else {
    _Where a : count(4);
  }
  x = 3;
}
// CHECK-LABEL: Function: different_redeclarations_after_where
//
// CHECK: Block: B4; Pred: B5; Succ: B3, B2
// CHECK: Gen [B4, AllSucc]: {}
// CHECK: Kill [B4, AllSucc]: {}
// CHECK: In [B4]:
// CHECK:   a: bounds(a, a + 2)
// CHECK: Out [B4, AllSucc]:
// CHECK:   a: bounds(a, a + 2)
// CHECK: Gen [B4 -> B3]:
// CHECK:   x != 0
// CHECK: Out [B4 -> B3]:
// CHECK-DAG:   x != 0
// CHECK-DAG:   a: bounds(a, a + 2)
// CHECK: Gen [B4 -> B2]:
// CHECK:   x == 0
// CHECK: Out [B4 -> B2]:
// CHECK-DAG:   x == 0
// CHECK-DAG:   a: bounds(a, a + 2)
//
// CHECK: Block: B1; Pred: B2, B3; Succ: B0
// CHECK: Gen [B1, AllSucc]: {}
// CHECK: Kill [B1, AllSucc]: x
// CHECK: In [B1]: {}
// CHECK: Out [B1, AllSucc]: {}

void different_redeclarations_after_type(int x, _Array_ptr<int> a : count(2)) {
  if (x) {
    _Where a : count(3);
  } else {
    _Where a : count(4);
  }
  x = 3;
}
// CHECK-LABEL: Function: different_redeclarations_after_type
//
// CHECK: Block: B4; Pred: B5; Succ: B3, B2
// CHECK: Gen [B4, AllSucc]: {}
// CHECK: Kill [B4, AllSucc]: {}
// CHECK: In [B4]:
// CHECK:   a: bounds(a, a + 2)
// CHECK: Out [B4, AllSucc]:
// CHECK:   a: bounds(a, a + 2)
// CHECK: Gen [B4 -> B3]:
// CHECK:   x != 0
// CHECK: Out [B4 -> B3]:
// CHECK-DAG:   x != 0
// CHECK-DAG:   a: bounds(a, a + 2)
// CHECK: Gen [B4 -> B2]:
// CHECK:   x == 0
// CHECK: Out [B4 -> B2]:
// CHECK-DAG:   x == 0
// CHECK-DAG:   a: bounds(a, a + 2)
//
// CHECK: Block: B1; Pred: B2, B3; Succ: B0
// CHECK: Gen [B1, AllSucc]: {}
// CHECK: Kill [B1, AllSucc]: x
// CHECK: In [B1]: {}
// CHECK: Out [B1, AllSucc]: {}

void not_the_same_redeclaration(int x, _Array_ptr<int> a : count(2)) {
  if (x) {
    _Where a : count(3);
  } else {
    _Array_ptr<int> a : count(3) = a;
  }
  x = 3;
}
// CHECK-LABEL: Function: not_the_same_redeclaration
//
// CHECK: Block: B4; Pred: B5; Succ: B3, B2
// CHECK: Gen [B4, AllSucc]: {}
// CHECK: Kill [B4, AllSucc]: {}
// CHECK: In [B4]:
// CHECK-DAG:   a: bounds(a, a + 2)
// CHECK: Out [B4, AllSucc]:
// CHECK-DAG:   a: bounds(a, a + 2)
// CHECK: Gen [B4 -> B3]:
// CHECK-DAG:   x != 0
// CHECK: Out [B4 -> B3]:
// CHECK-DAG:   x != 0
// CHECK-DAG:   a: bounds(a, a + 2)
// CHECK: Gen [B4 -> B2]:
// CHECK-DAG:   x == 0
// CHECK: Out [B4 -> B2]:
// CHECK-DAG:   x == 0
// CHECK-DAG:   a: bounds(a, a + 2)
//
// CHECK: Block: B3; Pred: B4; Succ: B1
// CHECK: Gen [B3, AllSucc]:
// CHECK:   a: bounds(a, a + 3)
// CHECK: Kill [B3, AllSucc]: a
// CHECK: In [B3]:
// CHECK-DAG:   x != 0
// CHECK-DAG:   a: bounds(a, a + 2)
// CHECK: Out [B3, AllSucc]:
// CHECK-DAG:   a: bounds(a, a + 3)
// CHECK-DAG:   x != 0
// CHECK: Gen [B3 -> B1]: {}
// CHECK: Out [B3 -> B1]:
// CHECK-DAG:   x != 0
//
// CHECK: Block: B2; Pred: B4; Succ: B1
// CHECK: Gen [B2, AllSucc]:
// CHECK:   a: bounds(a, a + 3)
// CHECK: Kill [B2, AllSucc]: a
// CHECK: In [B2]:
// CHECK-DAG:   x == 0
// CHECK-DAG:   a: bounds(a, a + 2)
// CHECK: Out [B2, AllSucc]:
// This is not the same `a`
// CHECK-DAG:   a: bounds(a, a + 3)
// CHECK-DAG:   x == 0
// CHECK-DAG:   a: bounds(a, a + 2)
// CHECK: Gen [B2 -> B1]: {}
// CHECK: Out [B2 -> B1]:
// CHECK-DAG:   x == 0
// CHECK-DAG:   a: bounds(a, a + 2)
//
// CHECK: Block: B1; Pred: B2, B3; Succ: B0
// CHECK: Gen [B1, AllSucc]: {}
// CHECK: Kill [B1, AllSucc]: x
// CHECK: In [B1]: {}
// CHECK: Out [B1, AllSucc]: {}
