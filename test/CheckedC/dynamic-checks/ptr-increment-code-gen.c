
// RUN: %clang_cc1 -fcheckedc-extension %s -ast-dump | FileCheck %s --check-prefix=CHECK-AST
// RUN: %clang_cc1 -fcheckedc-extension %s -emit-llvm -O0 -o - | FileCheck %s --check-prefix=CHECK-IR

// In the following generated IR, we do not verify the alignment of any loads/stores
// ie, the IR checked by line 37 might read "%1 = load i32*, i32** @gp1, align 4"
// but we discard the "align 4" at the end. This is because alignment is type-size
// dependent, which in turn is very platform dependent.
// It is better to elide these checks, because we really care about the order of
// loads, checks and stores, in these tests, not their specific alignment.

struct S1 {
  int f1;
};

extern _Ptr<int> gp1;
extern _Ptr<struct S1> gp2;

// CHECK-AST: FunctionDecl {{.*}} f1
// CHECK-IR: define {{.*}} void @f1
void f1(void) {
  //
  // Global Pointers
  //

  (*gp1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: UnaryOperator {{.*}} 'int' lvalue prefix '*'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp1' '_Ptr<int>'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]]
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG2]], 1
  // CHECK-IR-NEXT: store i32 [[REG3]], i32* [[REG1]]

  (gp2->f1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: MemberExpr {{.*}} ->f1
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp2' '_Ptr<struct S1>'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** @gp2
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG1]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]]
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG3]], 1
  // CHECK-IR-NEXT: store i32 [[REG4]], i32* [[REG2]]
}

// CHECK-AST: FunctionDecl {{.*}} f2
// CHECK-IR: define {{.*}} void @f2
void f2(_Ptr<int> lp1,
        _Ptr<struct S1> lp2) {
  //
  // Local Pointers
  //

  (*lp1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: UnaryOperator {{.*}} 'int' lvalue prefix '*'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp1' '_Ptr<int>'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]]
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG2]], 1
  // CHECK-IR-NEXT: store i32 [[REG3]], i32* [[REG1]]

  (lp2->f1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: MemberExpr {{.*}} ->f1
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp2' '_Ptr<struct S1>'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %lp2.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG1]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]]
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG3]], 1
  // CHECK-IR-NEXT: store i32 [[REG4]], i32* [[REG2]]
}
