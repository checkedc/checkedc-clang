
// RUN: %clang_cc1 -fcheckedc-extension -verify -verify-ignore-unexpected=note %s -ast-dump | FileCheck %s --check-prefix=CHECK-AST
// RUN: %clang_cc1 -fcheckedc-extension -verify -verify-ignore-unexpected=note %s -emit-llvm -O0 -o - | FileCheck %s --check-prefix=CHECK-IR

// In the following generated IR, we do not verify the alignment of any loads/stores
// ie, the IR checked by line 37 might read "%1 = load i32*, i32** @gp1, align 4"
// but we discard the "align 4" at the end. This is because alignment is type-size
// dependent, which in turn is very platform dependent.
// It is better to elide these checks, because we really care about the order of
// loads, checks and stores, in these tests, not their specific alignment.

extern _Array_ptr<int> gp1 : count(1);
extern _Array_ptr<int> gp3 : count(3);

extern int ga1 _Checked[1];
extern int ga3 _Checked[3];

extern int gma _Checked[3][3];

// CHECK-AST: FunctionDecl {{.*}} f1
// CHECK-IR: define {{.*}}void @f1
void f1(void) {
  //
  // Global Array_Ptrs
  //

  int x1 = gp1[0];
  // CHECK-AST: VarDecl {{.*}} x1 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscript
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG4]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG3]], [[REG2]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG2]], [[REG5]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]]
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %x1

  int x2 = *gp1;
  // CHECK-AST: VarDecl {{.*}} x2 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG3]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG2]], [[REG1]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG1]], [[REG4]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]]
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %x2

  int x3 = gp3[0];
  // CHECK-AST: VarDecl {{.*}} x3 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscript
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG4]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG3]], [[REG2]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG2]], [[REG5]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]]
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %x3

  int x4 = gp3[2];
  // CHECK-AST: VarDecl {{.*}} x4 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscript
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG4]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG3]], [[REG2]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG2]], [[REG5]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]]
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %x4

  int x5 = *gp3;
  // CHECK-AST: VarDecl {{.*}} x5 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG3]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG2]], [[REG1]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG1]], [[REG4]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]]
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %x5

  // CHECK-IR: ret void
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
}

// CHECK-AST: FunctionDecl {{.*}} f2
// CHECK-IR: define {{.*}}void @f2
void f2(_Array_ptr<int> lp1 : count(1),
        _Array_ptr<int> lp3 : count(3)) {
  //
  // Local Array_Ptrs
  //

  int y1 = lp1[0];
  // CHECK-AST: VarDecl {{.*}} y1 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscript
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG4]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG3]], [[REG2]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG2]], [[REG5]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]]
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %y1

  int y2 = *lp1;
  // CHECK-AST: VarDecl {{.*}} y2 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG3]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG2]], [[REG1]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG1]], [[REG4]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]]
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %y2

  int y3 = lp3[0];
  // CHECK-AST: VarDecl {{.*}} y3 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscript
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG4]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG3]], [[REG2]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG2]], [[REG5]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]]
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %y3

  int y4 = lp3[2];
  // CHECK-AST: VarDecl {{.*}} y4 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscript
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG4]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG3]], [[REG2]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG2]], [[REG5]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]]
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %y4

  int y5 = *lp3;
  // CHECK-AST: VarDecl {{.*}} y5 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG3]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG2]], [[REG1]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG1]], [[REG4]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]]
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %y5

  // CHECK-IR: ret void
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
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

  int x1 = ga1[0];
  // CHECK-AST: VarDecl {{.*}} x1 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscript
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'int _Checked[1]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'int _Checked[1]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1

  // NOTE: No Non-null check inserted
  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR:      br i1 icmp ult (
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([1 x i32], [1 x i32]* @ga1, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:   i32* getelementptr inbounds (i32,
  // CHECK-IR-SAME:     i32* getelementptr inbounds ([1 x i32], [1 x i32]* @ga1, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:     {{i[0-9]+}} 1)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]],
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([1 x i32], [1 x i32]* @ga1, {{i[0-9]+}} 0, {{i[0-9]+}} 0)
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %x1

  int x2 = *ga1;
  // CHECK-AST: VarDecl {{.*}} x2 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'int _Checked[1]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'int _Checked[1]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1

  // NOTE: No Non-null check inserted
  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR:      br i1 icmp ult (
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([1 x i32], [1 x i32]* @ga1, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:   i32* getelementptr inbounds (i32,
  // CHECK-IR-SAME:     i32* getelementptr inbounds ([1 x i32], [1 x i32]* @ga1, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:     {{i[0-9]+}} 1)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]],
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([1 x i32], [1 x i32]* @ga1, {{i[0-9]+}} 0, {{i[0-9]+}} 0)
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %x2

  int x3 = ga3[0];
  // CHECK-AST: VarDecl {{.*}} x3 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscript
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int _Checked[3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int _Checked[3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3

  // NOTE: No Non-null check inserted
  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR:      br i1 icmp ult (
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:   i32* getelementptr inbounds (i32,
  // CHECK-IR-SAME:     i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:     {{i[0-9]+}} 3)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]],
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0)
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %x3

  int x4 = ga3[2];
  // CHECK-AST: VarDecl {{.*}} x4 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscript
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int _Checked[3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int _Checked[3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3

  // NOTE: No Non-null check inserted
  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR:      br i1 icmp ult (
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 2),
  // CHECK-IR-SAME:   i32* getelementptr inbounds (i32,
  // CHECK-IR-SAME:     i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:     {{i[0-9]+}} 3)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]],
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 2)
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %x4

  int x5 = *ga3;
  // CHECK-AST: VarDecl {{.*}} x5 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int _Checked[3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int _Checked[3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3

  // NOTE: No Non-null check inserted
  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR:      br i1 icmp ult (
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:   i32* getelementptr inbounds (i32,
  // CHECK-IR-SAME:     i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:     {{i[0-9]+}} 3)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]],
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0)
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %x5

  // CHECK-IR: ret void
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
}

// CHECK-AST: FunctionDecl {{.*}} f4
// CHECK-IR: define {{.*}}void @f4
void f4(int la1 _Checked[1],
        int la3 _Checked[3]) {
  //
  // Local Checked Arrays
  //

  int y1 = la1[0];
  // CHECK-AST: VarDecl {{.*}} y1 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscript
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG4]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG3]], [[REG2]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG2]], [[REG5]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]]
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %y1

  int y2 = *la1;
  // CHECK-AST: VarDecl {{.*}} y2 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG3]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG2]], [[REG1]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG1]], [[REG4]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]]
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %y2

  int y3 = la3[0];
  // CHECK-AST: VarDecl {{.*}} y3 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscript
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG4]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG3]], [[REG2]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG2]], [[REG5]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]]
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %y3

  int y4 = la3[2];
  // CHECK-AST: VarDecl {{.*}} y4 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscript
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG4]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG3]], [[REG2]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG2]], [[REG5]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]]
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %y4

  int y5 = *la3;
  // CHECK-AST: VarDecl {{.*}} y5 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG3]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG2]], [[REG1]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG1]], [[REG4]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]]
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %y5

  // CHECK-IR: ret void
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
}

// CHECK-AST: FunctionDecl {{.*}} f5
// CHECK-IR: define {{.*}}void @f5
void f5() {
  //
  // Global Multidimensional Checked Arrays
  //

  int z1 = gma[0][0];
  // CHECK-AST: VarDecl {{.*}} z1 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscriptExpr
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int _Checked[3][3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int _Checked[3][3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // NOTE: No Non-null check inserted
  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR:      br i1 icmp ult (
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([3 x i32],
  // CHECK-IR-SAME:     [3 x i32]* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:     {{i[0-9]+}} 3, {{i[0-9]+}} 0)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]],
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0, {{i[0-9]+}} 0)
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %z1

  int z2 = gma[2][2];
  // CHECK-AST: VarDecl {{.*}} z2 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscriptExpr
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int _Checked[3][3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int _Checked[3][3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // NOTE: No Non-null check inserted
  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR:      br i1 icmp ult (
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} 2),
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([3 x i32],
  // CHECK-IR-SAME:     [3 x i32]* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:     {{i[0-9]+}} 3, {{i[0-9]+}} 0)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]],
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} 2)
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %z2

  // Valid Array Access
  int z3 = gma[0][5]; // expected-warning {{index 5 is past the end of the array}}
  // CHECK-AST: VarDecl {{.*}} z3 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscriptExpr
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int _Checked[3][3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int _Checked[3][3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // NOTE: No Non-null check inserted
  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR:      br i1 icmp ult (
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 1, {{i[0-9]+}} 2),
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([3 x i32],
  // CHECK-IR-SAME:     [3 x i32]* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:     {{i[0-9]+}} 3, {{i[0-9]+}} 0)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]],
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 1, {{i[0-9]+}} 2)
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %z3

  // Also Valid
  int z4 = gma[2][-2]; // expected-warning {{index -2 is before the beginning of the array}}
  // CHECK-AST: VarDecl {{.*}} z4 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscriptExpr
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int _Checked[3][3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int _Checked[3][3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // NOTE: No Non-null check inserted
  // The following line of LLVM IR is very long
  // CHECK-IR:      br i1 and (
  // CHECK-IR-SAME:   i1 icmp ule (
  // CHECK-IR-SAME:     i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:     i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} -2)),
  // CHECK-IR-SAME:   i1 icmp ult (
  // CHECK-IR-SAME:     i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} -2),
  // CHECK-IR-SAME:     i32* getelementptr inbounds ([3 x i32],
  // CHECK-IR-SAME:       [3 x i32]* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:       {{i[0-9]+}} 3, {{i[0-9]+}} 0))),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]],
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} -2)
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %z4

  int z5 = **gma;
  // CHECK-AST: VarDecl {{.*}} z5 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} '*'
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int _Checked[3][3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int _Checked[3][3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} '*'

  // NOTE: No Non-null check inserted
  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR:      br i1 icmp ult (
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([3 x i32],
  // CHECK-IR-SAME:     [3 x i32]* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:     {{i[0-9]+}} 3, {{i[0-9]+}} 0)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]],
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0, {{i[0-9]+}} 0)
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %z5

  int z6 = *gma[2];
  // CHECK-AST: VarDecl {{.*}} z6 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} '*'
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int _Checked[3][3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int _Checked[3][3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // NOTE: No Non-null check inserted
  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR:      br i1 icmp ult (
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:   i32* getelementptr inbounds ([3 x i32],
  // CHECK-IR-SAME:     [3 x i32]* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME:     {{i[0-9]+}} 3, {{i[0-9]+}} 0)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]],
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} 0)
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %z6

  // CHECK-IR: ret void
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
}

// CHECK-AST: FunctionDecl {{.*}} f6
// CHECK-IR: define {{.*}}void @f6
void f6(int lma _Checked[3][3]) {

  int z1 = lma[0][0];
  // CHECK-AST: VarDecl {{.*}} z1 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscriptExpr
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG2]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG2]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = bitcast [3 x i32]* [[REG4]] to i32*
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG6]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG6]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG8:%[a-zA-Z0-9.]*]] = bitcast [3 x i32]* [[REG7]] to i32*
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG5]], [[REG3]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG3]], [[REG8]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG3]]
  // CHECK-IR-NEXT: store i32 [[REG7]], i32* %z1

  int z2 = lma[2][2];
  // CHECK-AST: VarDecl {{.*}} z2 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscriptExpr
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG2]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG2]], {{i[0-9]+}} 0, {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = bitcast [3 x i32]* [[REG4]] to i32*
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG6]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG6]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG8:%[a-zA-Z0-9.]*]] = bitcast [3 x i32]* [[REG7]] to i32*
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG5]], [[REG3]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG3]], [[REG8]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG3]]
  // CHECK-IR-NEXT: store i32 [[REG7]], i32* %z2

  // Valid Array Access
  int z3 = lma[0][5];  // expected-warning {{index 5 is past the end of the array}}
  // CHECK-AST: VarDecl {{.*}} z3 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscriptExpr
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG2]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG2]], {{i[0-9]+}} 0, {{i[0-9]+}} 5
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = bitcast [3 x i32]* [[REG4]] to i32*
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG6]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG6]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG8:%[a-zA-Z0-9.]*]] = bitcast [3 x i32]* [[REG7]] to i32*
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG5]], [[REG3]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG3]], [[REG8]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG3]]
  // CHECK-IR-NEXT: store i32 [[REG7]], i32* %z3

  // Also Valid
  int z4 = lma[2][-2]; // expected-warning {{index -2 is before the beginning of the array}}
  // CHECK-AST: VarDecl {{.*}} z4 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscriptExpr
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG2]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG2]], {{i[0-9]+}} 0, {{i[0-9]+}} -2
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = bitcast [3 x i32]* [[REG4]] to i32*
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG6]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG6]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG8:%[a-zA-Z0-9.]*]] = bitcast [3 x i32]* [[REG7]] to i32*
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG5]], [[REG3]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG3]], [[REG8]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG3]]
  // CHECK-IR-NEXT: store i32 [[REG7]], i32* %z4

  int z5 = **lma;
  // CHECK-AST: VarDecl {{.*}} z5 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} '*'
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} '*'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG1]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG2]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = bitcast [3 x i32]* [[REG3]] to i32*
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG5]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG5]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = bitcast [3 x i32]* [[REG6]] to i32*
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG4]], [[REG2]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG2]], [[REG7]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]]
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %z5

  int z6 = *lma[2];
  // CHECK-AST: VarDecl {{.*}} z6 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} '*'
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int _Checked[3]>':'_Array_ptr<int _Checked[3]>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG1]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG2]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i32* [[REG3]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = bitcast [3 x i32]* [[REG4]] to i32*
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne [3 x i32]* [[REG6]], null
  // CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG6]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG8:%[a-zA-Z0-9.]*]] = bitcast [3 x i32]* [[REG7]] to i32*
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = icmp ule i32* [[REG5]], [[REG3]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = icmp ult i32* [[REG3]], [[REG8]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOW]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG7:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG3]]
  // CHECK-IR-NEXT: store i32 [[REG7]], i32* %z6

  // CHECK-IR: ret void
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
}

void f10(int index) _Checked {
  char c = 0;
  c = "abcdef"[index];

// CHECK-AST: BinaryOperator {{.*}} 'char' '='
// CHECK-AST-NEXT:    DeclRefExpr {{.*}} 'char' lvalue Var {{.*}} 'c' 'char'
// CHECK-AST-NEXT:    ImplicitCastExpr {{.*}} 'char' <LValueToRValue>
// CHECK-AST-NEXT:       ArraySubscriptExpr {{.*}} 'char' lvalue
// CHECK-AST-NEXT:         Bounds Null-terminated read
// CHECK-AST-NEXT:          RangeBoundsExpr {{.*}} 'NULL TYPE'
// CHECK-AST-NEXT:            ImplicitCastExpr {{.*}} '_Nt_array_ptr<char>':'_Nt_array_ptr<char>' <ArrayToPointerDecay>
// CHECK-AST-NEXT:            BoundsValueExpr  {{.*}} 'char _Nt_checked[7]' lvalue _BoundTemporary  [[TEMP1:[a-zA-Z0-9.]*]]
// CHECK-AST-NEXT:            BinaryOperator {{.*}} '_Nt_array_ptr<char>':'_Nt_array_ptr<char>' '+'
// CHECK-AST-NEXT:              ImplicitCastExpr {{.*}} '_Nt_array_ptr<char>':'_Nt_array_ptr<char>' <ArrayToPointerDecay>
// CHECK-AST-NEXT:                BoundsValueExpr {{.*}} 'char _Nt_checked[7]' lvalue _BoundTemporary [[TEMP1]]
// CHECK-AST-NEXT:              IntegerLiteral {{.*}} 'int' 6
// CHECK-AST-NEXT:         ImplicitCastExpr {{.*}} '_Nt_array_ptr<char>' <ArrayToPointerDecay>
// CHECK-AST-NEXT:          CHKCBindTemporaryExpr [[TEMP1]] <col:7> 'char _Nt_checked[7]' lvalue
// CHECK-AST-NEXT:            StringLiteral {{.*}} 'char _Nt_checked[7]' lvalue "abcdef"
// CHECK-AST-NEXT:         ImplicitCastExpr {{.*}} 'int' <LValueToRValue>
// CHECK-AST-NEXT:           DeclRefExpr {{.*}} 'int' lvalue ParmVar {{.*}} 'index' 'int'

// CHECK-IR: define {{.*}}void @f10
// CHECK-IR:        [[ARRAYIDX1:%[a-zA-Z0-9.]*]] = getelementptr inbounds [7 x i8], [7 x i8]* [[LITERAL1:@[^,]*]], {{i[0-9]+}} 0, {{i[0-9]+}} [[REG0:%[a-zA-Z0-9.]*]]
// CHECK-IR-NEXT:   [[CHECKLOWER1:%[a-zA-Z0-9._]*]] = icmp ule i8* getelementptr inbounds ([7 x i8], [7 x i8]* [[LITERAL1]], i64 0, i64 0), [[ARRAYIDX1]]
// CHECK-IR-NEXT:   [[CHECKUPPER1:%[a-zA-Z0-9._]*]] = icmp ule i8* [[ARRAYIDX1]], getelementptr inbounds ([7 x i8], [7 x i8]* [[LITERAL1]], i64 0, {{i[0-9]+}} 6)
// CHECK-IR-NEXT:   [[DYN_CHK_RANGE1:%[a-zA-Z0-9._]*]] = and i1 [[CHECKLOWER1]], [[CHECKUPPER1]]
// CHECK-IR-NEXT:   br i1 [[DYN_CHK_RANGE1]], label %[[DYNCHK_SUCC1:[a-zA-Z0-9._]*]], label %[[DYNCHK_FAIL1:[a-zA-Z0-9._]*]]
// CHECK-IR: [[DYNCHK_SUCC1]]:
  c = (char _Checked[]) { 'a', 'b', } [index];

// CHECK-AST:          BinaryOperator {{.*}} 'char' '='
// CHECK-AST-NEXT:      DeclRefExpr {{.*}} 'char' lvalue Var {{.*}} 'c' 'char'
// CHECK-AST-NEXT:      ImplicitCastExpr {{.*}} 'char' <LValueToRValue>
// CHECK-AST-NEXT:        ArraySubscriptExpr {{.*}} 'char' lvalue
// CHECK-AST-NEXT:          Bounds Normal
// CHECK-AST-NEXT:           RangeBoundsExpr {{.*}} 'NULL TYPE'
// CHECK-AST-NEXT:             ImplicitCastExpr {{.*}} '_Array_ptr<char>':'_Array_ptr<char>' <ArrayToPointerDecay>
// CHECK-AST-NEXT:              BoundsValueExpr {{.*}} 'char _Checked[2]' lvalue _BoundTemporary  [[TEMP2:[a-zA-Z0-9.]*]]
// CHECK-AST-NEXT:             BinaryOperator {{.*}} '_Array_ptr<char>':'_Array_ptr<char>' '+'
// CHECK-AST-NEXT:               ImplicitCastExpr {{.*}} '_Array_ptr<char>':'_Array_ptr<char>' <ArrayToPointerDecay>
// CHECK-AST-NEXT:                BoundsValueExpr {{.*}} 'char _Checked[2]' lvalue _BoundTemporary  [[TEMP2]]
// CHECK-AST-NEXT:               IntegerLiteral {{.*}} 'int' 2
// CHECK-AST-NEXT:          ImplicitCastExpr {{.*}} '_Array_ptr<char>' <ArrayToPointerDecay>
// CHECK-AST-NEXT:           CHKCBindTemporaryExpr [[TEMP2]] {{.*}} 'char _Checked[2]' lvalue
// CHECK-AST-NEXT:             CompoundLiteralExpr {{.*}} 'char _Checked[2]' lvalue
// CHECK-AST-NEXT:               InitListExpr {{.*}} 'char _Checked[2]'
// CHECK-AST-NEXT:                 ImplicitCastExpr {{.*}} 'char' <IntegralCast>
// CHECK-AST-NEXT:                  CharacterLiteral {{.*}} 'int' 97
// CHECK-AST-NEXT:                 ImplicitCastExpr {{.*}} 'char' <IntegralCast>
// CHECK-AST-NEXT:                   CharacterLiteral {{.*}} 'int' 98
// CHECK-AST-NEXT:          ImplicitCastExpr {{.*}} 'int' <LValueToRValue>
// CHECK-AST-NEXT:            DeclRefExpr {{.*}} 'int' lvalue ParmVar {{.*}} 'index' 'int'

// CHECK-IR: [[ARRAYIDX3:%[a-zA-Z0-9._]*]] = getelementptr inbounds [2 x i8], [2 x i8]* [[LITERAL2:%[^,]*]], {{i[0-9]+}} 0, {{i[0-9]+}} [[REG1:%[a-zA-Z0-9._]*]]
// CHECK-IR-NEXT: [[ARRAYDECAY:%[a-zA-Z0-9._]*]] = getelementptr inbounds [2 x i8], [2 x i8]* [[LITERAL2]], i64 0, i64 0
// CHECK-IR-NEXT: [[ARRAYDECAY4:%[a-zA-Z0-9._]*]] = getelementptr inbounds [2 x i8], [2 x i8]* [[LITERAL2]], i64 0, i64 0
// CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[ARRAYDECAY4]], null
// CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
// CHECK-IR: [[LAB_DYSUC]]:
// CHECK-IR-NEXT: [[ADDPTR1:%[a-zA-Z0-9._]*]] = getelementptr inbounds i8, i8* [[ARRAYDECAY4]], {{i[0-9]+}} 2
// CHECK-IR-NEXT: [[LOWER1:%[a-zA-Z0-9._]*]] = icmp ule i8* [[ARRAYDECAY]], [[ARRAYIDX3]]
// CHECK-IR-NEXT: [[UPPER1:%[a-zA-Z0-9._]*]] = icmp ult i8* [[ARRAYIDX3]], [[ADDPTR1]]
// CHECK-IR-NEXT: [[DYN_CHK_RANGE1:%[a-zA-Z0-9._]*]] = and i1 [[LOWER1]], [[UPPER1]]
// CHECK-IR-NEXT: br i1 [[DYN_CHK_RANGE1]], label %[[DYNCHK_SUCC2:[a-zA-Z0-9._]*]], label %[[DYNCHK_FAIL2:[a-zA-Z0-9._]*]]
// CHECK-IR: [[DYNCHK_SUCC2]]:

  c = (char _Nt_checked[]) { 'a', '\0' } [index];

// CHECK-AST:          BinaryOperator {{.*}} 'char' '='
// CHECK-AST-NEXT:       DeclRefExpr {{.*}} 'char' lvalue Var {{.*}} 'c' 'char'
// CHECK-AST-NEXT:       ImplicitCastExpr {{.*}} 'char' <LValueToRValue>
// CHECK-AST-NEXT:         ArraySubscriptExpr {{.*}} 'char' lvalue
// CHECK-AST-NEXT:           Bounds Null-terminated read
// CHECK-AST-NEXT:            RangeBoundsExpr {{.*}} 'NULL TYPE'
// CHECK-AST-NEXT:              ImplicitCastExpr {{.*}} '_Nt_array_ptr<char>':'_Nt_array_ptr<char>' <ArrayToPointerDecay>
// CHECK-AST-NEXT:               BoundsValueExpr {{.*}} 'char _Nt_checked[2]' lvalue _BoundTemporary  [[TEMP3:[a-zA-Z0-9.]*]]
// CHECK-AST-NEXT:              BinaryOperator{{.*}} '_Nt_array_ptr<char>':'_Nt_array_ptr<char>' '+'
// CHECK-AST-NEXT:                ImplicitCastExpr {{.*}} '_Nt_array_ptr<char>':'_Nt_array_ptr<char>' <ArrayToPointerDecay>
// CHECK-AST-NEXT:                 BoundsValueExpr {{.*}} 'char _Nt_checked[2]' lvalue _BoundTemporary  [[TEMP3]]
// CHECK-AST-NEXT:                IntegerLiteral{{.*}} 'int' 1
// CHECK-AST-NEXT:           ImplicitCastExpr {{.*}} '_Nt_array_ptr<char>' <ArrayToPointerDecay>
// CHECK-AST-NEXT:            CHKCBindTemporaryExpr [[TEMP3]] {{.*}} 'char _Nt_checked[2]' lvalue
// CHECK-AST-NEXT:              CompoundLiteralExpr {{.*}} 'char _Nt_checked[2]' lvalue
// CHECK-AST-NEXT:                InitListExpr {{.*}} 'char _Nt_checked[2]'
// CHECK-AST-NEXT:                  ImplicitCastExpr {{.*}} 'char' <IntegralCast>
// CHECK-AST-NEXT:                   CharacterLiteral {{.*}} 'int' 97
// CHECK-AST-NEXT:                  ImplicitCastExpr {{.*}} 'char' <IntegralCast>
// CHECK-AST-NEXT:                    CharacterLiteral {{.*}} 'int' 0
// CHECK-AST-NEXT:           ImplicitCastExpr{{.*}} 'int' <LValueToRValue>
// CHECK-AST-NEXT:             DeclRefExpr {{.*}} 'int' lvalue ParmVar {{.*}} 'index' 'int'

// CHECK-IR: [[ARRAYIDX16:%[a-zA-Z0-9._]*]] = getelementptr inbounds [2 x i8], [2 x i8]* [[LITERAL3:%[^,]*]], {{i[0-9]+}} 0, {{i[0-9]+}} [[REG1:%[a-zA-Z0-9._]*]]
// CHECK-IR: [[ARRAYDECAY17:%[a-zA-Z0-9._]*]] = getelementptr inbounds [2 x i8], [2 x i8]* [[LITERAL3]], i64 0, i64 0
// CHECK-IR: [[ARRAYDECAY18:%[a-zA-Z0-9._]*]] = getelementptr inbounds [2 x i8], [2 x i8]* [[LITERAL3]], i64 0, i64 0
// CHECK-IR-NEXT: [[REG_DYNN:%_Dynamic_check.non_null[a-zA-Z0-9.]*]] = icmp ne i8* [[ARRAYDECAY18]], null
// CHECK-IR-NEXT: br i1 [[REG_DYNN]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
// CHECK-IR: [[LAB_DYSUC]]:
// CHECK-IR-NEXT: [[ADDPTR2:%[a-zA-Z0-9._]*]] = getelementptr inbounds i8, i8* [[ARRAYDECAY18]], {{i[0-9]+}} 1
// CHECK-IR-NEXT: [[LOWER2:%[a-zA-Z0-9._]*]] = icmp ule i8* [[ARRAYDECAY17]], [[ARRAYIDX16]]
// CHECK-IR-NEXT: [[UPPER2:%[a-zA-Z0-9._]*]] = icmp ule i8* [[ARRAYIDX16]], [[ADDPTR2]]
// CHECK-IR-NEXT: [[DYN_CHK_RANGE2:%[a-zA-Z0-9._]*]] = and i1 [[LOWER2]], [[UPPER2]]
// CHECK-IR-NEXT: br i1 [[DYN_CHK_RANGE2]], label %[[DYNCHK_SUCC3:[a-zA-Z0-9._]*]], label %[[DYNCHK_FAIL3:[a-zA-Z0-9._]*]]
// CHECK-IR: [[DYNCHK_SUCC3]]:

// CHECK-IR: [[DYNCHK_FAIL1]]:
// CHECK-IR: call void @llvm.trap()
// CHECK-IR: [[DYNCHK_FAIL2]]:
// CHECK-IR: call void @llvm.trap()
// CHECK-IR: [[DYNCHK_FAIL3]]:
// CHECK-IR: call void @llvm.trap()
}
