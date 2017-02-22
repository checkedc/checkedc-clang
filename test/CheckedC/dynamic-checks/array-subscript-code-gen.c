
// RUN: %clang_cc1 -fcheckedc-extension %s -ast-dump | FileCheck %s --check-prefix=CHECK-AST
// RUN: %clang_cc1 -fcheckedc-extension %s -emit-llvm -O0 -o - | FileCheck %s --check-prefix=CHECK-IR

_Array_ptr<int> ga1 : count(1) = 0;
_Array_ptr<int> ga3 : count(3) = 0;

int gca1 _Checked[1] = { 1 };
int gca3 _Checked[3] = { 0, 1, 2 };

// CHECK-AST: FunctionDecl {{.*}} func 'void (_Array_ptr<int> : count(1), _Array_ptr<int> : count(3))'
// CHECK-IR: define void @func
void func(_Array_ptr<int> lp1 : count(1), _Array_ptr<int> lp3 : count(3)) {

  //
  // Global Array_Ptrs
  //
  
  int x1 = ga1[0];
  // CHECK-AST: VarDecl {{.*}} x1 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1
  // CHECK-AST-NEXT: ArraySubscript

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga1, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], i32 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga1, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga1, align 4
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

  int x2 = ga3[0];
  // CHECK-AST: VarDecl {{.*}} x2 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ArraySubscript

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga3, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], i32 0
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga3, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga3, align 4
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
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %x2, align 4

  int x3 = ga3[2];
  // CHECK-AST: VarDecl {{.*}} x3 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: ArraySubscript

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga3, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = getelementptr inbounds i32, i32* [[REG1]], i32 2
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga3, align 4
  // CHECK-IR-NEXT: [[REG4:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga3, align 4
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

  int x4 = *ga1;
  // CHECK-AST: VarDecl {{.*}} x4 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga1' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 1
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga1, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga1, align 4
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga1, align 4
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
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %x4, align 4

  int x5 = *ga3;
  // CHECK-AST: VarDecl {{.*}} x5 'int' cinit
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: Inferred Bounds
  // CHECK-AST-NEXT: RangeBoundsExpr
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: BinaryOperator {{.*}} '_Array_ptr<int>' '+'
  // CHECK-AST-NEXT: ImplicitCastExpr {{.*}} <LValueToRValue>
  // CHECK-AST-NEXT: DeclRefExpr {{.*}} 'ga3' '_Array_ptr<int>'
  // CHECK-AST-NEXT: IntegerLiteral {{.*}} 'int' 3
  // CHECK-AST-NEXT: UnaryOperator {{.*}} prefix '*'

  // CHECK-IR: [[REG1:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga3, align 4
  // CHECK-IR-NEXT: [[REG2:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga3, align 4
  // CHECK-IR-NEXT: [[REG3:%[a-zA-Z0-9.]*]] = load i32*, i32** @ga3, align 4
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

  int y2 = lp3[0];
  // CHECK-AST: VarDecl {{.*}} y2 'int' cinit
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
  // CHECK-IR-NEXT: store i32 [[REG6]], i32* %y2, align 4

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

  int y4 = *lp1;
  // CHECK-AST: VarDecl {{.*}} y4 'int' cinit
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
  // CHECK-IR-NEXT: store i32 [[REG5]], i32* %y4, align 4

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

  // We should have 10 bounds checks
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