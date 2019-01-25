// Tests for dumping of ASTS with Checked C _Dynamic_checks
//
// RUN: %clang_cc1 -ast-dump -fcheckedc-extension %s | FileCheck %s

//===================================================================
// Dumps of _Dynamic_check
//===================================================================

void f1(_Ptr<int> i) {
  _Dynamic_check(1);
  // CHECK: CallExpr
  // CHECK: DeclRefExpr
  // CHECK-SAME: <builtin fn type>
  // CHECK-SAME: _Dynamic_check

  _Dynamic_check(0);
  // CHECK: CallExpr
  // CHECK: _Dynamic_check
  // CHECK-NEXT: IntegerLiteral

  _Dynamic_check(i != 0);
  // CHECK: CallExpr
  // CHECK: _Dynamic_check
  // CHECK-NEXT: BinaryOperator
  // CHECK-SAME: '!='

  _Dynamic_check(i == 0);
  // CHECK: CallExpr
  // CHECK: _Dynamic_check
  // CHECK-NEXT: BinaryOperator
  // CHECK-SAME: '=='
 
  _Dynamic_check(-1);
  // CHECK: CallExpr
  // CHECK: _Dynamic_check
  // CHECK-NEXT: UnaryOperator
  // CHECK-SAME: '-'

  int j;
  _Dynamic_check(j);
  // CHECK: CallExpr
  // CHECK: _Dynamic_check
  // CHECK: DeclRefExpr
}