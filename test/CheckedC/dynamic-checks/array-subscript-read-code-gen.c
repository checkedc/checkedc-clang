
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
// CHECK-IR: define void @f1
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
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG1]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG4]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG1]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG4]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
}

// CHECK-AST: FunctionDecl {{.*}} f2
// CHECK-IR: define void @f2
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
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG1]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG4]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG1]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG4]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
}

// CHECK-AST: FunctionDecl {{.*}} f3
// CHECK-IR: define void @f3
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'int checked[1]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'int checked[1]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 1

  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR: br i1 icmp slt ({{i[0-9]+}} ptrtoint ([1 x i32]* @ga1 to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint (i32* getelementptr inbounds
  // CHECK-IR-SAME: (i32, i32* getelementptr inbounds ([1 x i32], [1 x i32]* @ga1, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME: {{i[0-9]+}} 1) to {{i[0-9]+}})),
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'int checked[1]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'int checked[1]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 1

  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR: br i1 icmp slt ({{i[0-9]+}} ptrtoint ([1 x i32]* @ga1 to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint (i32* getelementptr inbounds
  // CHECK-IR-SAME: (i32, i32* getelementptr inbounds ([1 x i32], [1 x i32]* @ga1, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME: {{i[0-9]+}} 1) to {{i[0-9]+}})),
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int checked[3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int checked[3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3

  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR: br i1 icmp slt ({{i[0-9]+}} ptrtoint ([3 x i32]* @ga3 to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint (i32* getelementptr inbounds
  // CHECK-IR-SAME: (i32, i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME: {{i[0-9]+}} 3) to {{i[0-9]+}})),
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int checked[3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int checked[3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3

  // The following line of LLVM IR is very long
  // CHECK-IR: br i1 and
  // CHECK-IR-SAME: (i1 icmp sle ({{i[0-9]+}} ptrtoint ([3 x i32]* @ga3 to {{i[0-9]+}}), {{i[0-9]+}} ptrtoint (i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 2) to {{i[0-9]+}})),
  // CHECK-IR-SAME: i1 icmp slt ({{i[0-9]+}} ptrtoint (i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 2) to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint (i32* getelementptr inbounds (i32, i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0), {{i[0-9]+}} 3) to {{i[0-9]+}}))),
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int checked[3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int checked[3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3

  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR: br i1 icmp slt ({{i[0-9]+}} ptrtoint ([3 x i32]* @ga3 to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint (i32* getelementptr inbounds
  // CHECK-IR-SAME: (i32, i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME: {{i[0-9]+}} 3) to {{i[0-9]+}})),
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
// CHECK-IR: define void @f4
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
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 1


  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 1
  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], {{i[0-9]+}} 1
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG1]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG4]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG1]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG4]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]]
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %y5


  // CHECK-IR: ret void
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
}



// CHECK-AST: FunctionDecl {{.*}} f5
// CHECK-IR: define void @f5
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int checked[3][3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int checked[3][3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR: br i1 icmp slt ({{i[0-9]+}} ptrtoint ([3 x [3 x i32]]* @gma to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint ([3 x i32]* getelementptr inbounds
  // CHECK-IR-SAME: ([3 x i32], [3 x i32]* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME: {{i[0-9]+}} 3) to {{i[0-9]+}})),
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int checked[3][3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int checked[3][3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // The following line of LLVM IR is very long
  // CHECK-IR: br i1 and
  // CHECK-IR-SAME: (i1 icmp sle ({{i[0-9]+}} ptrtoint ([3 x [3 x i32]]* @gma to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint (i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} 2) to {{i[0-9]+}})),
  // CHECK-IR-SAME: i1 icmp slt ({{i[0-9]+}} ptrtoint (i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} 2) to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint ([3 x i32]* getelementptr inbounds
  // CHECK-IR-SAME: ([3 x i32], [3 x i32]* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0), {{i[0-9]+}} 3) to {{i[0-9]+}}))),
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int checked[3][3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int checked[3][3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // The following line of LLVM IR is very long
  // CHECK-IR: br i1 and
  // CHECK-IR-SAME: (i1 icmp sle ({{i[0-9]+}} ptrtoint ([3 x [3 x i32]]* @gma to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint (i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 1, {{i[0-9]+}} 2) to {{i[0-9]+}})),
  // CHECK-IR-SAME: i1 icmp slt ({{i[0-9]+}} ptrtoint (i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 1, {{i[0-9]+}} 2) to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint ([3 x i32]* getelementptr inbounds
  // CHECK-IR-SAME: ([3 x i32], [3 x i32]* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0), {{i[0-9]+}} 3) to {{i[0-9]+}}))),
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int checked[3][3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int checked[3][3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // The following line of LLVM IR is very long
  // CHECK-IR: br i1 and
  // CHECK-IR-SAME: (i1 icmp sle ({{i[0-9]+}} ptrtoint ([3 x [3 x i32]]* @gma to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint (i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} -2) to {{i[0-9]+}})),
  // CHECK-IR-SAME: i1 icmp slt ({{i[0-9]+}} ptrtoint (i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} -2) to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint ([3 x i32]* getelementptr inbounds
  // CHECK-IR-SAME: ([3 x i32], [3 x i32]* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0), {{i[0-9]+}} 3) to {{i[0-9]+}}))),
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int checked[3][3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int checked[3][3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} '*'
  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is very long
  // CHECK-IR: br i1 icmp slt ({{i[0-9]+}} ptrtoint ([3 x [3 x i32]]* @gma to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint ([3 x i32]* getelementptr inbounds
  // CHECK-IR-SAME: ([3 x i32], [3 x i32]* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0),
  // CHECK-IR-SAME: {{i[0-9]+}} 3) to {{i[0-9]+}})),
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int checked[3][3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gma' 'int checked[3][3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // The following line of LLVM IR is very long
  // CHECK-IR: br i1 and
  // CHECK-IR-SAME: (i1 icmp sle ({{i[0-9]+}} ptrtoint ([3 x [3 x i32]]* @gma to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint (i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} 0) to {{i[0-9]+}})),
  // CHECK-IR-SAME: i1 icmp slt ({{i[0-9]+}} ptrtoint (i32* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 2, {{i[0-9]+}} 0) to {{i[0-9]+}}),
  // CHECK-IR-SAME: {{i[0-9]+}} ptrtoint ([3 x i32]* getelementptr inbounds
  // CHECK-IR-SAME: ([3 x i32], [3 x i32]* getelementptr inbounds ([3 x [3 x i32]], [3 x [3 x i32]]* @gma, {{i[0-9]+}} 0, {{i[0-9]+}} 0), {{i[0-9]+}} 3) to {{i[0-9]+}}))),
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
// CHECK-IR: define void @f6
void f6(int lma _Checked[3][3]) {

  int z1 = lma[0][0];
  // CHECK-AST: VarDecl {{.*}} z1 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: ArraySubscriptExpr
  // CHECK-AST-NEXT: Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG2]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG5]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint [3 x i32]* [[REG4]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint [3 x i32]* [[REG6]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG2]], {{i[0-9]+}} 0, {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG5]], i32 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint [3 x i32]* [[REG4]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint [3 x i32]* [[REG6]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG1]], {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG2]], {{i[0-9]+}} 0, {{i[0-9]+}} 5
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG5]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint [3 x i32]* [[REG4]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint [3 x i32]* [[REG6]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG2]], {{i[0-9]+}} 0, {{i[0-9]+}} -2
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG5]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint [3 x i32]* [[REG4]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint [3 x i32]* [[REG6]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: UnaryOperator {{.*}} '*'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG1]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG4]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint [3 x i32]* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint [3 x i32]* [[REG5]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lma' '_Array_ptr<int checked[3]>':'_Array_ptr<int checked[3]>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: ArraySubscriptExpr

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG1]], {{i[0-9]+}} 2
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG2]], {{i[0-9]+}} 0, {{i[0-9]+}} 0
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load [3 x i32]*, [3 x i32]** %lma.addr
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = getelementptr inbounds [3 x i32], [3 x i32]* [[REG5]], {{i[0-9]+}} 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint [3 x i32]* [[REG4]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle {{i[0-9]+}} [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint [3 x i32]* [[REG6]] to {{i[0-9]+}}
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt {{i[0-9]+}} [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
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
}

