
// RUN: %clang_cc1 -fcheckedc-extension %s -ast-dump | FileCheck %s --check-prefix=CHECK-AST
// RUN: %clang_cc1 -fcheckedc-extension %s -emit-llvm -O0 -o - | FileCheck %s --check-prefix=CHECK-IR

// In the following generated IR, we do not verify the alignment of any loads/stores
// ie, the IR checked by line 37 might read "%1 = load i32*, i32** @gp1, align 4"
// but we discard the "align 4" at the end. This is because alignment is type-size
// dependent, which in turn is very platform dependent.
// It is better to elide these checks, because we really care about the order of
// loads, checks and stores, in these tests, not their specific alignment.

typedef struct {
  int f1;
} S1;

extern _Array_ptr<S1> gp1 : count(1);
extern _Array_ptr<S1> gp3 : count(3);

extern S1 ga1 _Checked[1];
extern S1 ga3 _Checked[3];

// CHECK-AST: FunctionDecl {{.*}} f1
// CHECK-IR: define {{.*}}void @f1
void f1(void) {
  //
  // Global Array_Ptrs
  //

  (gp1->f1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: MemberExpr {{.*}} ->f1
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp1' '_Array_ptr<S1>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<S1>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp1' '_Array_ptr<S1>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp1' '_Array_ptr<S1>'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** @gp1
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** @gp1
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** @gp1
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG3]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG3]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule %struct.S1* [[REG2]], [[REG1]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult %struct.S1* [[REG1]], [[REG4]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG1]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG5]]
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG6]], 1
  // CHECK-IR-NEXT: store i32 [[REG7]], i32* [[REG5]]

  (gp3->f1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: MemberExpr {{.*}} ->f1
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<S1>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<S1>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<S1>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<S1>'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** @gp3
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** @gp3
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** @gp3
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG3]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG3]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule %struct.S1* [[REG2]], [[REG1]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult %struct.S1* [[REG1]], [[REG4]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG1]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG5]]
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG6]], 1
  // CHECK-IR-NEXT: store i32 [[REG7]], i32* [[REG5]]

  ((gp3 + 2)->f1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: MemberExpr {{.*}} ->f1
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<S1>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<S1>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<S1>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: BinaryOperator
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<S1>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 2

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** @gp3
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG2]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** @gp3
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** @gp3
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG4]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule %struct.S1* [[REG3]], [[REG2]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult %struct.S1* [[REG2]], [[REG5]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG2]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG6]]
  // CHECK-IR-NEXT: [[REG8:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG7]], 1
  // CHECK-IR-NEXT: store i32 [[REG8]], i32* [[REG6]]

  // CHECK-IR: ret void
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
}

// CHECK-AST: FunctionDecl {{.*}} f2
// CHECK-IR: define {{.*}}void @f2
void f2(_Array_ptr<S1> lp1 : count(1),
        _Array_ptr<S1> lp3 : count(3)) {
  //
  // Local Array_Ptrs
  //

  (lp1->f1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: MemberExpr {{.*}} ->f1
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp1' '_Array_ptr<S1>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<S1>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp1' '_Array_ptr<S1>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp1' '_Array_ptr<S1>'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %lp1.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %lp1.addr
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %lp1.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG3]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG3]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule %struct.S1* [[REG2]], [[REG1]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult %struct.S1* [[REG1]], [[REG4]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG1]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG5]]
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG6]], 1
  // CHECK-IR-NEXT: store i32 [[REG7]], i32* [[REG5]]


  (lp3->f1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: MemberExpr {{.*}} ->f1
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<S1>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<S1>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<S1>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<S1>'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %lp3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %lp3.addr
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %lp3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG3]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG3]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule %struct.S1* [[REG2]], [[REG1]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult %struct.S1* [[REG1]], [[REG4]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG1]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG5]]
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG6]], 1
  // CHECK-IR-NEXT: store i32 [[REG7]], i32* [[REG5]]


  ((lp3+2)->f1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: MemberExpr {{.*}} ->f1
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<S1>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<S1>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<S1>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: BinaryOperator
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<S1>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 2

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %lp3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG2]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %lp3.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %lp3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG4]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule %struct.S1* [[REG3]], [[REG2]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult %struct.S1* [[REG2]], [[REG5]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG2]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG6]]
  // CHECK-IR-NEXT: [[REG8:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG7]], 1
  // CHECK-IR-NEXT: store i32 [[REG8]], i32* [[REG6]]


  // CHECK-IR: ret void
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
}

// CHECK-AST: FunctionDecl {{.*}} f3
// CHECK-IR: define {{.*}}void @f3
void f3(void) {
  //
  // Global Checked Arrays
  //

  (ga1->f1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: MemberExpr {{.*}} ->f1
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'S1 _Checked[1]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<S1>':'_Array_ptr<S1>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'S1 _Checked[1]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'S1 _Checked[1]'

  // NOTE: No Non-null check inserted
  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR:      br i1 icmp ult (
  // CHECK-IR-SAME:   %struct.S1* getelementptr inbounds ([1 x %struct.S1], [1 x %struct.S1]* @ga1, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:   %struct.S1* getelementptr inbounds (%struct.S1,
  // CHECK-IR-SAME:     %struct.S1* getelementptr inbounds ([1 x %struct.S1], [1 x %struct.S1]* @ga1, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:     {{i[0-9]+}} 1)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]],
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([1 x %struct.S1], [1 x %struct.S1]* @ga1, {{i[0-9]+}} 0, {{i[0-9]+}} 0, {{i[0-9]+}} 0)
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG6]], 1
  // CHECK-IR-NEXT: store i32 [[REG7]], i32* getelementptr inbounds ([1 x %struct.S1], [1 x %struct.S1]* @ga1, {{i[0-9]+}} 0, {{i[0-9]+}} 0, {{i[0-9]+}} 0)

  (ga3->f1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: MemberExpr {{.*}} ->f1
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'S1 _Checked[3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<S1>':'_Array_ptr<S1>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'S1 _Checked[3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'S1 _Checked[3]'

  // NOTE: No Non-null check inserted
  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR:      br i1 icmp ult (
  // CHECK-IR-SAME:   %struct.S1* getelementptr inbounds ([3 x %struct.S1], [3 x %struct.S1]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:   %struct.S1* getelementptr inbounds (%struct.S1,
  // CHECK-IR-SAME:     %struct.S1* getelementptr inbounds ([3 x %struct.S1], [3 x %struct.S1]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:     {{i[0-9]+}} 3)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]],
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([3 x %struct.S1], [3 x %struct.S1]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0, {{i[0-9]+}} 0)
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG6]], 1
  // CHECK-IR-NEXT: store i32 [[REG7]], i32* getelementptr inbounds ([3 x %struct.S1], [3 x %struct.S1]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0, {{i[0-9]+}} 0)

  ((ga3+2)->f1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: MemberExpr {{.*}} ->f1
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'S1 _Checked[3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<S1>':'_Array_ptr<S1>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'S1 _Checked[3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'S1 _Checked[3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 2

  // NOTE: No Non-null check inserted
  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR:      br i1 icmp ult (
  // CHECK-IR-SAME:   %struct.S1* getelementptr inbounds ([3 x %struct.S1], [3 x %struct.S1]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 2),
  // CHECK-IR-SAME:   %struct.S1* getelementptr inbounds (%struct.S1,
  // CHECK-IR-SAME:     %struct.S1* getelementptr inbounds ([3 x %struct.S1], [3 x %struct.S1]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:     {{i[0-9]+}} 3)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]],
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([3 x %struct.S1], [3 x %struct.S1]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} 0)
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG6]], 1
  // CHECK-IR-NEXT: store i32 [[REG7]], i32* getelementptr inbounds ([3 x %struct.S1], [3 x %struct.S1]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} 0)

  // CHECK-IR: ret void
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
}

// CHECK-AST: FunctionDecl {{.*}} f4
// CHECK-IR: define {{.*}}void @f4
void f4(S1 la1 _Checked[1],
        S1 la3 _Checked[3]) {
  //
  // Local Checked Arrays
  //

  (la1->f1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: MemberExpr {{.*}} ->f1
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la1' '_Array_ptr<S1>':'_Array_ptr<S1>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<S1>':'_Array_ptr<S1>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la1' '_Array_ptr<S1>':'_Array_ptr<S1>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la1' '_Array_ptr<S1>':'_Array_ptr<S1>'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %la1.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %la1.addr
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %la1.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG3]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG3]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule %struct.S1* [[REG2]], [[REG1]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult %struct.S1* [[REG1]], [[REG4]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG1]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG5]]
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG6]], 1
  // CHECK-IR-NEXT: store i32 [[REG7]], i32* [[REG5]]

  (la3->f1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: MemberExpr {{.*}} ->f1
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<S1>':'_Array_ptr<S1>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<S1>':'_Array_ptr<S1>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<S1>':'_Array_ptr<S1>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<S1>':'_Array_ptr<S1>'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %la3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %la3.addr
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %la3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG3]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG3]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule %struct.S1* [[REG2]], [[REG1]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult %struct.S1* [[REG1]], [[REG4]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG1]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG5]]
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG6]], 1
  // CHECK-IR-NEXT: store i32 [[REG7]], i32* [[REG5]]

  ((la3+2)->f1)++;
  // CHECK-AST: UnaryOperator {{.*}} 'int' postfix '++'
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: MemberExpr {{.*}} ->f1
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<S1>':'_Array_ptr<S1>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<S1>':'_Array_ptr<S1>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<S1>':'_Array_ptr<S1>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ParenExpr
  // CHECK-AST-NEXT: BinaryOperator
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<S1>':'_Array_ptr<S1>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 2

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %la3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG2]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %la3.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load %struct.S1*, %struct.S1** %la3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne %struct.S1* [[REG4]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule %struct.S1* [[REG3]], [[REG2]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult %struct.S1* [[REG2]], [[REG5]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = getelementptr inbounds %struct.S1, %struct.S1* [[REG2]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG6]]
  // CHECK-IR-NEXT: [[REG8:%[a-zA-Z0-9.]*]] = add nsw i32 [[REG7]], 1
  // CHECK-IR-NEXT: store i32 [[REG8]], i32* [[REG6]]

  // CHECK-IR: ret void
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
}
