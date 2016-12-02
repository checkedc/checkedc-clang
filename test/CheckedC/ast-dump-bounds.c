// Tests for dumping of ASTS with Checked C extensions.
// This makes sure that additional information appears as
// expected.
//
// RUN: %clang_cc1 -ast-dump -fcheckedc-extension %s | FileCheck %s

//===================================================================
// Dumps of different kinds of bounds expressions on global variables
//===================================================================

_Array_ptr<int> g_arr1 : count(5);

// CHECK: VarDecl
// CHECK: g_arr1 '_Array_ptr<int>'
// CHECK-NEXT: CountBoundsExpr
// CHECK: Element
// CHECK-NEXT: IntegerLiteral
// CHECK: 'int' 5

_Array_ptr<int> g_arr2 : byte_count(sizeof(int) * 5);

// CHECK: VarDecl
// CHECK: g_arr2 '_Array_ptr<int>'
// CHECK-NEXT: CountBoundsExpr
// CHECK: Byte
// CHECK-NEXT: BinaryOperator
// CHECK: IntegerLiteral
// CHECK: 'int' 5

_Array_ptr<int> g_arr3 : bounds(g_arr3, g_arr3 + 5);

// CHECK: VarDecl
// CHECK: g_arr3 '_Array_ptr<int>'
// CHECK-NEXT: RangeBoundsExpr
// CHECK-NEXT: ImplicitCastExpr
// CHECK-NEXT: DeclRefExpr
// CHECK: g_arr3
// CHECK-NEXT: BinaryOperator
// CHECK-NEXT: ImplicitCastExpr
// CHECK: g_arr3
// CHECK-NEXT: IntegerLiteral
// CHECK: 'int' 5

int * g_arr4 : itype(_Ptr<int>);

// CHECK: VarDecl
// CHECK: g_arr4 'int *'
// CHECK-NEXT: InteropTypeBoundsAnnotation
// CHECK: '_Ptr<int>'

//===================================================================
// Dumps of different kinds of bounds expressions on local variables
//===================================================================

void f1() {
  _Array_ptr<int> arr1 : count(5) = 0;

// CHECK: VarDecl
// CHECK: arr1 '_Array_ptr<int>'
// CHECK-NEXT: CountBoundsExpr
// CHECK: Element
// CHECK-NEXT: IntegerLiteral
// CHECK: 'int' 5

  _Array_ptr<int> arr2 : byte_count(sizeof(int) * 5) = 0;

// CHECK: VarDecl
// CHECK: arr2 '_Array_ptr<int>'
// CHECK-NEXT: CountBoundsExpr
// CHECK: Byte
// CHECK-NEXT: BinaryOperator
// CHECK: IntegerLiteral
// CHECK: 'int' 5

  _Array_ptr<int> arr3 : bounds(arr3, arr3 + 5) = 0;

// CHECK: VarDecl
// CHECK: arr3 '_Array_ptr<int>'
// CHECK-NEXT: RangeBoundsExpr
// CHECK-NEXT: ImplicitCastExpr
// CHECK-NEXT: DeclRefExpr
// CHECK: arr3
// CHECK-NEXT: BinaryOperator
// CHECK-NEXT: ImplicitCastExpr
// CHECK: arr3
// CHECK-NEXT: IntegerLiteral
// CHECK: 'int' 5

  int * arr4 : itype(_Ptr<int>) = 0;

// CHECK: VarDecl
// CHECK: arr4 'int *'
// CHECK-NEXT: InteropTypeBoundsAnnotation
// CHECK: '_Ptr<int>'
}

//=============================================================
// Dumps of different kinds of bounds expressions on parameters
//=============================================================

void f10(_Array_ptr<int> arr1 : count(5));
// CHECK: FunctionDecl
// CHECK: f10
// CHECK-NEXT: ParmVarDecl
// CHECK: arr1 '_Array_ptr<int>'
// CHECK-NEXT: CountBoundsExpr
// CHECK: Element

void f11(_Array_ptr<int> arr1 : byte_count(sizeof(int) * 5));

// CHECK: FunctionDecl
// CHECK: f11
// CHECK-NEXT: ParmVarDecl
// CHECK: arr1 '_Array_ptr<int>'
// CHECK-NEXT: CountBoundsExpr
// CHECK: Byte

void f12(_Array_ptr<int> arr1 : bounds(arr1, arr1 + 5));

// CHECK: FunctionDecl
// CHECK: f12
// CHECK-NEXT: ParmVarDecl
// CHECK: arr1 '_Array_ptr<int>'
// CHECK-NEXT: RangeBoundsExpr
// CHECK-NEXT: ImplicitCastExpr
// CHECK-NEXT: DeclRefExpr
// CHECK: arr1
// CHECK-NEXT: BinaryOperator
// CHECK-NEXT: ImplicitCastExpr
// CHECK: arr1
// CHECK-NEXT: IntegerLiteral
// CHECK: 'int' 5

void f13(int *pint : itype(_Ptr<int>));

// CHECK: FunctionDecl
// CHECK: f13
// CHECK-NEXT: ParmVarDecl
// CHECK: pint 'int *'
// CHECK-NEXT: InteropTypeBoundsAnnotation
// CHECK: '_Ptr<int>'

void f14(int arr1 _Checked[] : count(5));
// CHECK: FunctionDecl
// CHECK: f14
// CHECK-NEXT: ParmVarDecl
// CHECK: arr1 '_Array_ptr<int>'
// CHECK-NEXT: CountBoundsExpr
// CHECK: Element
// CHECK-NEXT: IntegerLiteral
// CHECK 'int' 5

//===================================================================
// Dumps of different kinds of bounds expressions on function returns
//===================================================================

_Array_ptr<int> f20(void) : count(5);
// CHECK: FunctionDecl
// CHECK: 20
// CHECK-NEXT: CountBoundsExpr
// CHECK: Element
// CHECK-NEXT: IntegerLiteral
// CHECK 'int' 5

_Array_ptr<int> f21(void) : byte_count(sizeof(int) * 5);

// CHECK: FunctionDecl
// CHECK: f21
// CHECK-NEXT: CountBoundsExpr
// CHECK: Byte
// CHECK: IntegerLiteral
// CHECK 'int' 5

_Array_ptr<int> f22(_Array_ptr<int> arr1 : count(5)) : bounds(arr1, arr1 + 5);

// CHECK: FunctionDecl
// CHECK: f22
// CHECK: RangeBoundsExpr
// CHECK-NEXT: ImplicitCastExpr
// CHECK-NEXT: DeclRefExpr
// CHECK: arr1
// CHECK-NEXT: BinaryOperator
// CHECK-NEXT: ImplicitCastExpr
// CHECK: arr1
// CHECK-NEXT: IntegerLiteral
// CHECK: 'int' 5

int *f23(void) : itype(_Ptr<int>);

// CHECK: FunctionDecl
// CHECK: f23
// CHECK: 'int *(void)'
// CHECK-NEXT: InteropTypeBoundsAnnotation
// CHECK: '_Ptr<int>'

//===================================================================
// Dumps of different kinds of bounds expressions on structure members
//===================================================================

struct S1 {
  _Array_ptr<int> arr1 : count(5);

  // CHECK: FieldDecl
  // CHECK: arr1 '_Array_ptr<int>'
  // CHECK-NEXT: CountBoundsExpr
  // CHECK: Element
  // CHECK-NEXT: IntegerLiteral
  // CHECK: 'int' 5

  _Array_ptr<int> arr2 : byte_count(sizeof(int) * 5);

  // CHECK: FieldDecl
  // CHECK: arr2 '_Array_ptr<int>'
  // CHECK-NEXT: CountBoundsExpr
  // CHECK: Byte
  // CHECK-NEXT: BinaryOperator
  // CHECK: IntegerLiteral
  // CHECK: 'int' 5

  _Array_ptr<int> arr3 : bounds(arr3, arr3 + 5);

  // CHECK: FieldDecl
  // CHECK: arr3 '_Array_ptr<int>'
  // CHECK-NEXT: RangeBoundsExpr
  // CHECK-NEXT: ImplicitCastExpr
  // CHECK-NEXT: DeclRefExpr
  // CHECK: arr3
  // CHECK-NEXT: BinaryOperator
  // CHECK-NEXT: ImplicitCastExpr
  // CHECK: arr3
  // CHECK-NEXT: IntegerLiteral
  // CHECK: 'int' 5

  int * arr4 : itype(_Ptr<int>);

  // CHECK: FieldDecl
  // CHECK: arr4 'int *'
  // CHECK-NEXT: InteropTypeBoundsAnnotation
  // CHECK: '_Ptr<int>'
};

//===================================================================
// Dumps of bounds expressions for function types
//===================================================================

void f30(_Array_ptr<int> arr : bounds(arr, arr + len), int len);

// CHECK: FunctionDecl
// CHECK: f30
// CHECK: 'void (_Array_ptr<int> : bounds(arg #0, arg #0 + arg #1), int)'
// CHECK-NEXT: ParmVarDecl
// CHECK: arr '_Array_ptr<int>'
// CHECK-NEXT: RangeBoundsExpr

void f31(int (*fn)(_Array_ptr<int> arr : bounds(arr, arr + len), int len));

// CHECK: FunctionDecl
// CHECK: f31
// CHECK: 'void (int (*)(_Array_ptr<int> : bounds(arg #0, arg #0 + arg #1), int))'
// CHECK: ParmVarDecl
// CHECK: fn
// CHECK: 'int (*)(_Array_ptr<int> : bounds(arg #0, arg #0 + arg #1), int)'

typedef float fn_sum(int lower, int upper,
                     _Array_ptr<float> arr : bounds(arr - lower, arr + upper));

// CHECK: TypedefDecl
// CHECK: fn_sum
// CHECK: 'float (int, int, _Array_ptr<float> : bounds(arg #2 - arg #0, arg #2 + arg #1))'
// CHECK-NEXT: FunctionProtoType
// CHECK: 'float (int, int, _Array_ptr<float> : bounds(arg #2 - arg #0, arg #2 + arg #1))'
// CHECK-NEXT: BuiltinType
// CHECK: float
// CHECK-NEXT: BuiltinType
// CHECK: int
// CHECK-NEXT: BuiltinType
// CHECK: int
// CHECK-NEXT: PointerType
// CHECK: _Array_ptr<float>
// CHECK-NEXT: BuiltinType
// CHECK: float

//
// range-expression for the _Array_ptr<float> parameter
//

// CHECK-NEXT: RangeBoundsExpr

// arg #2 - arg #0

// CHECK-NEXT: BinaryOperator
// CHECK: '_Array_ptr<float>'
// CHECK: '-'
// CHECK-NEXT: ImplicitCastExpr
// CHECK-NEXT: PositionalParameterExpr
// CHECK: arg
// CHECK: #2
// CHECK-NEXT: ImplicitCastExpr
// CHECK-NEXT: PositionalParameterExpr
// CHECK: arg
// CHECK: #0

// arg #2 + arg #1

// CHECK-NEXT: BinaryOperator
// CHECK: '_Array_ptr<float>'
// CHECK: '+'
// CHECK-NEXT: ImplicitCastExpr
// CHECK-NEXT: PositionalParameterExpr
// CHECK: arg
// CHECK: #2
// CHECK-NEXT: ImplicitCastExpr
// CHECK-NEXT: PositionalParameterExpr
// CHECK: arg
// CHECK: #1
