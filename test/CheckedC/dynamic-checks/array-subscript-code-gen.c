
// RUN: %clang_cc1 -fcheckedc-extension %s -ast-dump | FileCheck %s --check-prefix=CHECK-AST
// RUN: %clang_cc1 -fcheckedc-extension %s -emit-llvm -O0 -o - | FileCheck %s --check-prefix=CHECK-IR

_Array_ptr<int> gp1 : count(1) = 0;
_Array_ptr<int> gp3 : count(3) = 0;

int ga1 _Checked[1];
int ga3 _Checked[3];

// CHECK-AST: FunctionDecl {{.*}} f1
// CHECK-IR: define void @f1
void f1(void) {
  //
  // Global Array_Ptrs
  //

  int x1 = gp1[0];
  // CHECK-AST: VarDecl {{.*}} x1 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1
  // CHECK-AST-NEXT: ArraySubscript

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], i32 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1, align 4
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], i32 1
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]], align 4
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %x1, align 4

  int x2 = *gp1;
  // CHECK-AST: VarDecl {{.*}} x2 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1, align 4
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp1, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], i32 1
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG1]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG4]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]], align 4
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %x2, align 4

  int x3 = gp3[0];
  // CHECK-AST: VarDecl {{.*}} x3 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ArraySubscript

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], i32 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3, align 4
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], i32 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]], align 4
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %x3, align 4

  int x4 = gp3[2];
  // CHECK-AST: VarDecl {{.*}} x4 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ArraySubscript

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], i32 2
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3, align 4
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], i32 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]], align 4
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %x4, align 4

  int x5 = *gp3;
  // CHECK-AST: VarDecl {{.*}} x5 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'gp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3, align 4
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @gp3, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], i32 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG1]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG4]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]], align 4
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %x5, align 4


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
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1
  // CHECK-AST-NEXT: ArraySubscript

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], i32 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr, align 4
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], i32 1
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]], align 4
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %y1, align 4

  int y2 = *lp1;
  // CHECK-AST: VarDecl {{.*}} y2 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr, align 4
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp1.addr, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], i32 1
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG1]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG4]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]], align 4
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %y2, align 4

  int y3 = lp3[0];
  // CHECK-AST: VarDecl {{.*}} y3 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ArraySubscript

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], i32 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr, align 4
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], i32 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]], align 4
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %y3, align 4

  int y4 = lp3[0];
  // CHECK-AST: VarDecl {{.*}} y4 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ArraySubscript

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], i32 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr, align 4
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], i32 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]], align 4
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %y4, align 4

  int y5 = *lp3;
  // CHECK-AST: VarDecl {{.*}} y5 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'lp3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr, align 4
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %lp3.addr, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], i32 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG1]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG4]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]], align 4
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %y5, align 4

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
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'int checked[1]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'int checked[1]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 1
  // CHECK-AST-NEXT: ArraySubscript

  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is offensively long
  // CHECK-IR: br i1 icmp slt (i32 ptrtoint ([1 x i32]* @ga1 to i32),
  // CHECK-IR-SAME: i32 ptrtoint (i32* getelementptr inbounds 
  // CHECK-IR-SAME: (i32, i32* getelementptr inbounds ([1 x i32], [1 x i32]* @ga1, i32 0, i32 0),
  // CHECK-IR-SAME: i32 1) to i32)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], 
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([1 x i32], [1 x i32]* @ga1, i32 0, i32 0), align 4
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %x1, align 4

  int x2 = *ga1;
  // CHECK-AST: VarDecl {{.*}} x2 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'int checked[1]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' 'int checked[1]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 1
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'

  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is offensively long
  // CHECK-IR: br i1 icmp slt (i32 ptrtoint ([1 x i32]* @ga1 to i32),
  // CHECK-IR-SAME: i32 ptrtoint (i32* getelementptr inbounds 
  // CHECK-IR-SAME: (i32, i32* getelementptr inbounds ([1 x i32], [1 x i32]* @ga1, i32 0, i32 0),
  // CHECK-IR-SAME: i32 1) to i32)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], 
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([1 x i32], [1 x i32]* @ga1, i32 0, i32 0), align 4
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %x2, align 4

  int x3 = ga3[0];
  // CHECK-AST: VarDecl {{.*}} x3 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int checked[3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int checked[3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ArraySubscript

  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is offensively long
  // CHECK-IR: br i1 icmp slt (i32 ptrtoint ([3 x i32]* @ga3 to i32),
  // CHECK-IR-SAME: i32 ptrtoint (i32* getelementptr inbounds 
  // CHECK-IR-SAME: (i32, i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, i32 0, i32 0),
  // CHECK-IR-SAME: i32 3) to i32)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], 
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, i32 0, i32 0), align 4
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %x3, align 4

  int x4 = ga3[2];
  // CHECK-AST: VarDecl {{.*}} x4 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int checked[3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int checked[3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ArraySubscript

  // The following line of LLVM IR is offensively long
  // CHECK-IR: br i1 and
  // CHECK-IR-SAME: (i1 icmp sle (i32 ptrtoint ([3 x i32]* @ga3 to i32), i32 ptrtoint (i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, i32 0, i32 2) to i32)),
  // CHECK-IR-SAME: i1 icmp slt (i32 ptrtoint (i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, i32 0, i32 2) to i32), 
  // CHECK-IR-SAME: i32 ptrtoint (i32* getelementptr inbounds (i32, i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, i32 0, i32 0), i32 3) to i32))),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], 
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, i32 0, i32 2), align 4
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %x4, align 4

  int x5 = *ga3;
  // CHECK-AST: VarDecl {{.*}} x5 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int checked[3]'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <ArrayToPointerDecay>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' 'int checked[3]'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'

  // NOTE: Only (addr LT upper) check generated, constant folder removes (lower LE addr) check
  // The following line of LLVM IR is offensively long
  // CHECK-IR: br i1 icmp slt (i32 ptrtoint ([3 x i32]* @ga3 to i32),
  // CHECK-IR-SAME: i32 ptrtoint (i32* getelementptr inbounds 
  // CHECK-IR-SAME: (i32, i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, i32 0, i32 0),
  // CHECK-IR-SAME: i32 3) to i32)),
  // CHECK-IR-SAME: label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], 
  // CHECK-IR-SAME: label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG1:%[a-zA-Z0-9.]*]] = load i32, i32* getelementptr inbounds ([3 x i32], [3 x i32]* @ga3, i32 0, i32 0), align 4
  // CHECK-IR-NEXT: store i32 [[REG1]], i32* %x5, align 4


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
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 1
  // CHECK-AST-NEXT: ArraySubscript

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], i32 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr, align 4
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], i32 1
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]], align 4
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %y1, align 4

  int y2 = *la1;
  // CHECK-AST: VarDecl {{.*}} y2 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 1
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr, align 4
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la1.addr, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], i32 1
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG1]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG4]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]], align 4
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %y2, align 4

  int y3 = la3[0];
  // CHECK-AST: VarDecl {{.*}} y3 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ArraySubscript

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], i32 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr, align 4
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], i32 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]], align 4
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %y3, align 4

  int y4 = la3[2];
  // CHECK-AST: VarDecl {{.*}} y4 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: ArraySubscript

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], i32 2
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr, align 4
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG4]], i32 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG3]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG5]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG6:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG2]], align 4
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %y4, align 4

  int y5 = *la3;
  // CHECK-AST: VarDecl {{.*}} y5 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>':'_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'la3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'unsigned long long' 3
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr, align 4
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** %la3.addr, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG3]], i32 3
  // CHECK-IR-NEXT: [[REG_DYADDR:%_Dynamic_check.addr[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG1]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOW:%_Dynamic_check.lower[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG2]] to i32
  // CHECK-IR-NEXT: [[REG_DYLOWC:%_Dynamic_check.lower_cmp[a-zA-Z0-9.]*]] = icmp sle i32 [[REG_DYLOW]], [[REG_DYADDR]]
  // CHECK-IR-NEXT: [[REG_DYUPP:%_Dynamic_check.upper[a-zA-Z0-9.]*]] = ptrtoint i32* [[REG4]] to i32
  // CHECK-IR-NEXT: [[REG_DYUPPC:%_Dynamic_check.upper_cmp[a-zA-Z0-9.]*]] = icmp slt i32 [[REG_DYADDR]], [[REG_DYUPP]]
  // CHECK-IR-NEXT: [[REG_DYRNG:%_Dynamic_check.range[a-zA-Z0-9.]*]] = and i1 [[REG_DYLOWC]], [[REG_DYUPPC]]
  // CHECK-IR-NEXT: br i1 [[REG_DYRNG]], label %[[LAB_DYSUC:_Dynamic_check.succeeded[a-zA-Z0-9.]*]], label %{{_Dynamic_check.failed[a-zA-Z0-9.]*}}
  // CHECK-IR: [[LAB_DYSUC]]:
  // CHECK-IR-NEXT: [[REG5:%[a-zA-Z0-9.]*]] = load i32, i32* [[REG1]], align 4
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %y5, align 4


  // CHECK-IR: ret void
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
  // CHECK-IR: call void @llvm.trap
}