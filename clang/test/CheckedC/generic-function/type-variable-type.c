// Test to make sure the AST is correctly generated for generic types.
//
// More specifically, test the AST dump of generic function declaration and 
// generic function types associated to it. Generic types are bound to 
// TypedefDecl, which stores the identifier name for the generic type. Also,
// the TypeVariable is is stored inside of FunctionDecl, and should be dumped
// underneath FunctionDecl, before ParmVarDecl.
//
// Also, the test sees if TypeVariable types are correctly displaying for
// variables that has the type TypeVariable.
//
// RUN: %clang_cc1 -ast-dump -fcheckedc-extension %s | FileCheck %s

// Check declaration of a generic function

_For_any(T, S, R) _Ptr<T> f1(_Ptr<T> a, _Ptr<S> b, _Ptr<R> c) {
  // CHECK: TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced T '(0, 0)'
  // CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 0)'
  // CHECK-NEXT: TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced S '(0, 1)'
  // CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 1)'
  // CHECK-NEXT: TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced R '(0, 2)'
  // CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 2)'
  // CHECK-NEXT: FunctionDecl {{0x[0-9a-f]+}} {{.*}} used f1 '_For_any(3) _Ptr<T> (_Ptr<T>, _Ptr<S>, _Ptr<R>)'
  // CHECK-NEXT: TypeVariable {{0x[0-9a-f]+}} {{.*}} T '(0, 0)'
  // CHECK-NEXT: TypeVariable {{0x[0-9a-f]+}} {{.*}} S '(0, 1)'
  // CHECK-NEXT: TypeVariable {{0x[0-9a-f]+}} {{.*}} R '(0, 2)'
  // CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} {{.*}} used a '_Ptr<T>'
  // CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} {{.*}} b '_Ptr<S>'
  // CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} {{.*}} c '_Ptr<R>'
  return a;
  // CHECK: ReturnStmt {{0x[0-9a-f]+}}
  // CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}}'_Ptr<T>' <LValueToRValue>
  // CHECK-NEXT: DeclRefExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<T>' lvalue ParmVar {{0x[0-9a-f]+}} 'a' '_Ptr<T>'
}

void call_f1() {
  int i = 0;
  _Ptr<int> ip = &i;
  f1<int, int, int>(ip, ip, ip);
}

_Itype_for_any(T, S, R) void *f2(void *a : itype(_Ptr<T>), void *b : itype(_Ptr<S>), 
                                 void * c : itype(_Ptr<R>)) : itype(_Ptr<T>) {
  // CHECK: TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced T '(0, 0) __BoundsInterface'
  // CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 0)  __BoundsInterface'
  // CHECK-NEXT: TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced S '(0, 1) __BoundsInterface'
  // CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 1) __BoundsInterface'
  // CHECK-NEXT: TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced R '(0, 2) __BoundsInterface'
  // CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 2) __BoundsInterface'
  // CHECK-NEXT: FunctionDecl {{0x[0-9a-f]+}} {{.*}} used f2 '_Itype_for_any(3) void *(void * : itype(_Ptr<T>), void * : itype(_Ptr<S>), void * : itype(_Ptr<R>)) : itype(_Ptr<T>)'
  // CHECK-NEXT: TypeVariable {{0x[0-9a-f]+}} {{.*}} T '(0, 0) __BoundsInterface'
  // CHECK-NEXT: TypeVariable {{0x[0-9a-f]+}} {{.*}} S '(0, 1) __BoundsInterface'
  // CHECK-NEXT: TypeVariable {{0x[0-9a-f]+}} {{.*}} R '(0, 2) __BoundsInterface'
  // CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} {{.*}} used a 'void *'
  // CHECK-NEXT: InteropTypeExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<T>'
  // CHECK-NEXT:   PointerType {{0x[0-9a-f]+}} '_Ptr<T>'
  // CHECK-NEXT:     TypedefType {{0x[0-9a-f]+}} 'T' sugar
  // CHECK-NEXT:       Typedef {{0x[0-9a-f]+}} 'T'
  // CHECK-NEXT:         TypeVariableType {{0x[0-9a-f]+}} '(0, 0) __BoundsInterface'
  // CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} {{.*}} b 'void *'
  // CHECK-NEXT: InteropTypeExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<S>'
  // CHECK-NEXT:   PointerType {{0x[0-9a-f]+}} '_Ptr<S>'
  // CHECK-NEXT:     TypedefType {{0x[0-9a-f]+}} 'S' sugar
  // CHECK-NEXT:       Typedef {{0x[0-9a-f]+}} 'S'
  // CHECK-NEXT:         TypeVariableType {{0x[0-9a-f]+}} '(0, 1) __BoundsInterface'
  // CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} {{.*}} c 'void *'
  // CHECK-NEXT: InteropTypeExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<R>'
  // CHECK-NEXT:   PointerType {{0x[0-9a-f]+}} '_Ptr<R>'
  // CHECK-NEXT:     TypedefType {{0x[0-9a-f]+}} 'R' sugar
  // CHECK-NEXT:       Typedef {{0x[0-9a-f]+}} 'R'
  // CHECK-NEXT:         TypeVariableType {{0x[0-9a-f]+}} '(0, 2) __BoundsInterface'
  return a;
  // CHECK: ReturnStmt {{0x[0-9a-f]+}}
  // CHECK-NEXT: ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}}'void *' <LValueToRValue>
  // CHECK-NEXT: DeclRefExpr {{0x[0-9a-f]+}} {{.*}} 'void *' lvalue ParmVar {{0x[0-9a-f]+}} 'a' 'void *'
}

void call_f2() {
  int i = 0;
  _Ptr<int> ip = &i;
  f2<int, int, int>(ip, ip, ip);
}


// Generic function that takes a generic function as an argument.
_For_any(T, S) _Ptr<T> f3(_Ptr<_For_any(R) int(_Ptr<R>, _Ptr<R>)> cmp, _Ptr<T> a, _Ptr<S> b) {
   return 0;
}

// CHECK:      TypedefDecl {{0x[0-9a-f]+}} {{.*}}referenced T '(0, 0)'
// CHECK-NEXT:   TypeVariableType {{0x[0-9a-f]+}} '(0, 0)'
// CHECK-NEXT: TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced S '(0, 1)'
// CHECK-NEXT:   TypeVariableType {{0x[0-9a-f]+}} '(0, 1)'
// CHECK-NEXT: TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced R '(1, 0)'
// CHECK-NEXT:   TypeVariableType {{0x[0-9a-f]+}} '(1, 0)'
// CHECK-NEXT: FunctionDecl {{0x[0-9a-f]+}} {{.*}} f3 '_For_any(2) _Ptr<T> (_Ptr<_For_any(1) int (_Ptr<R>, _Ptr<R>)>, _Ptr<T>, _Ptr<S>)'
// CHECK-NEXT: TypeVariable {{0x[0-9a-f]+}} {{.*}} T '(0, 0)'
// CHECK-NEXT: TypeVariable {{0x[0-9a-f]+}} {{.*}} S '(0, 1)'
// CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} {{.*}} col:67 cmp '_Ptr<_For_any(1) int (_Ptr<R>, _Ptr<R>)>'
// CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} {{.*}} a '_Ptr<T>'
// CHECK-NEXT: ParmVarDecl {{0x[0-9a-f]+}} {{.*}} b '_Ptr<S>'
// CHECK-NEXT: CompoundStmt {{0x[0-9a-f]+}} {{.*}}
// CHECK-NEXT:   ReturnStmt {{0x[0-9a-f]+}} {{.*}}
// CHECK-NEXT:     ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<T>' <NullToPointer>
// CHECK-NEXT:    IntegerLiteral {{0x[0-9a-f]+}} {{.*}} 'int' 0
  
// Generic function that returns a generic function..
_For_any(T, S) _Ptr<_For_any(R) int(_Ptr<R>, _Ptr<R>)> f4(_Ptr<T> a, _Ptr<S> b) {
   return 0;
}

// CHECK:        TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced T '(0, 0)'
// CHECK-NEXT:     TypeVariableType {{0x[0-9a-f]+}} '(0, 0)'
// CHECK-NEXT:  TypedefDecl {{0x[0-9a-f]+}}  {{.*}} referenced S '(0, 1)'
// CHECK-NEXT:    TypeVariableType {{0x[0-9a-f]+}} '(0, 1)'
// CHECK-NEXT:  TypedefDecl {{0x[0-9a-f]+}}  {{.*}} referenced R '(1, 0)'
// CHECK-NEXT:    TypeVariableType {{0x[0-9a-f]+}} '(1, 0)'
// CHECK-NEXT:  FunctionDecl {{0x[0-9a-f]+}}  {{.*}} f4 '_For_any(2) _Ptr<_For_any(1) int (_Ptr<R>, _Ptr<R>)> (_Ptr<T>, _Ptr<S>)'
// CHECK-NEXT:    TypeVariable {{0x[0-9a-f]+}} {{.*}} T '(0, 0)'
// CHECK-NEXT:    TypeVariable {{0x[0-9a-f]+}} {{.*}} S '(0, 1)'
// CHECK-NEXT:    ParmVarDecl {{0x[0-9a-f]+}}  {{.*}} a '_Ptr<T>'
// CHECK-NEXT:    ParmVarDecl {{0x[0-9a-f]+}}  {{.*}} b '_Ptr<S>'
// CHECK-NEXT:    CompoundStmt {{0x[0-9a-f]+}} {{.*}}
// CHECK-NEXT:      ReturnStmt {{0x[0-9a-f]+}} {{.*}}
// CHECK-NEXT:        ImplicitCastExpr {{0x[0-9a-f]+}} {{.*}}'_Ptr<_For_any(1) int (_Ptr<R>, _Ptr<R>)>' <NullToPointer>
// CHECK-NEXT:          IntegerLiteral {{0x[0-9a-f]+}}  {{.*}} 'int' 0

// While we can typedef a polymorphic function type, we have no syntax for applying
// it right now within a type.  So have to copy out the signature for cmp.
_For_any(T) void reverse(_Array_ptr<T> p : count(len), int len,
                         unsigned size, _Ptr<int (_Ptr<T>, _Ptr<T>)> cmp);

_For_any(T) void sort(_Array_ptr<T> p : count(len), int len, unsigned size,
                      _Ptr<int (_Ptr<T>, _Ptr<T>)> cmp);

_For_any(T) void compose(_Array_ptr<T> p : count(len), int len, unsigned size,
                         _Ptr<_For_any(S) void (_Array_ptr<S> p : count(len), int len, unsigned size, _Ptr<int (_Ptr<S>, _Ptr<S>)> cmp)> f1,
                         _Ptr<_For_any(R) void (_Array_ptr<R> p : count(len), int len, unsigned size, _Ptr<int (_Ptr<R>, _Ptr<R>)> cmp)> f2,
                         _Ptr<int (_Ptr<T>, _Ptr<T>)> cmp);

int cmp(_Ptr<int> a, _Ptr<int> b) {
 return (*a - *b);
}

void test() {
   int a[5] = { 5, 3, 10, 3, 2 };
   compose<int>(a, 5, sizeof(int), reverse, sort, cmp);
// AST for call to compose.
// CHECK:   CallExpr {{.*}} 'void'
// CHECK-NEXT: ImplicitCastExpr {{.*}} 'void (*)(_Array_ptr<int> : count(arg #1), int, unsigned int, _Ptr<_For_any(1) void (_Array_ptr<(0, 0)> : count(arg #1), int, unsigned int, _Ptr<int (_Ptr<(0, 0)>, _Ptr<(0, 0)>)>)>, _Ptr<_For_any(1) void (_Array_ptr<(0, 0)> : count(arg #1), int, unsigned int, _Ptr<int (_Ptr<(0, 0)>, _Ptr<(0, 0)>)>)>, _Ptr<int (_Ptr<int>, _Ptr<int>)>)' <FunctionToPointerDecay>
// CHECK-NEXT:   DeclRefExpr {{.*}} 'void (_Array_ptr<int> : count(arg #1), int, unsigned int, _Ptr<_For_any(1) void (_Array_ptr<(0, 0)> : count(arg #1), int, unsigned int, _Ptr<int (_Ptr<(0, 0)>, _Ptr<(0, 0)>)>)>, _Ptr<_For_any(1) void (_Array_ptr<(0, 0)> : count(arg #1), int, unsigned int, _Ptr<int (_Ptr<(0, 0)>, _Ptr<(0, 0)>)>)>, _Ptr<int (_Ptr<int>, _Ptr<int>)>)' instantiated Function {{.*}} 'compose' '_For_any(1) void (_Array_ptr<T> : count(arg #1), int, unsigned int, _Ptr<_For_any(1) void (_Array_ptr<S> : count(arg #1), int, unsigned int, _Ptr<int (_Ptr<S>, _Ptr<S>)>)>, _Ptr<_For_any(1) void (_Array_ptr<R> : count(arg #1), int, unsigned int, _Ptr<int (_Ptr<R>, _Ptr<R>)>)>, _Ptr<int (_Ptr<T>, _Ptr<T>)>)'
// CHECK-NEXT:     BuiltinType {{.*}} 'int'
// CHECK-NEXT: ImplicitCastExpr {{.*}} '_Array_ptr<int>' <BitCast>
// CHECK-NEXT:   ImplicitCastExpr {{.*}} 'int *' <ArrayToPointerDecay>
// CHECK-NEXT:     DeclRefExpr {{.*}} 'int [5]' lvalue Var {{0x[0-9a-f]+}} 'a' 'int [5]'
// CHECK-NEXT: IntegerLiteral {{.*}} 'int' 5
// CHECK:       UnaryExprOrTypeTraitExpr {{.*}} '{{unsigned .*}}' sizeof 'int'
// CHECK-NEXT: ImplicitCastExpr {{.*}} '_Ptr<_For_any(1) void (_Array_ptr<(0, 0)> : count(arg #1), int, unsigned int, _Ptr<int (_Ptr<(0, 0)>, _Ptr<(0, 0)>)>)>' <BitCast>
// CHECK-NEXT:   ImplicitCastExpr {{.*}} '_For_any(1) void (*)(_Array_ptr<T> : count(arg #1), int, unsigned int, _Ptr<int (_Ptr<T>, _Ptr<T>)>)' <FunctionToPointerDecay>
// CHECK-NEXT:     DeclRefExpr {{.*}} '_For_any(1) void (_Array_ptr<T> : count(arg #1), int, unsigned int, _Ptr<int (_Ptr<T>, _Ptr<T>)>)' Function {{0x[0-9a-f]+}} 'reverse' '_For_any(1) void (_Array_ptr<T> : count(arg #1), int, unsigned int, _Ptr<int (_Ptr<T>, _Ptr<T>)>)
// CHECK-NEXT: ImplicitCastExpr {{.*}} '_Ptr<_For_any(1) void (_Array_ptr<(0, 0)> : count(arg #1), int, unsigned int, _Ptr<int (_Ptr<(0, 0)>, _Ptr<(0, 0)>)>)>' <BitCast>
// CHECK-NEXT:   ImplicitCastExpr {{.*}} '_For_any(1) void (*)(_Array_ptr<T> : count(arg #1), int, unsigned int, _Ptr<int (_Ptr<T>, _Ptr<T>)>)' <FunctionToPointerDecay>
// CHECK-NEXT:     DeclRefExpr {{.*}} '_For_any(1) void (_Array_ptr<T> : count(arg #1), int, unsigned int, _Ptr<int (_Ptr<T>, _Ptr<T>)>)' Function {{0x[0-9a-f]+}} 'sort' '_For_any(1) void (_Array_ptr<T> : count(arg #1), int, unsigned int, _Ptr<int (_Ptr<T>, _Ptr<T>)>)'
// CHECK-NEXT: ImplicitCastExpr {{.*}} '_Ptr<int (_Ptr<int>, _Ptr<int>)>' <BitCast>
// CHECK-NEXT:   ImplicitCastExpr{{.*}} 'int (*)(_Ptr<int>, _Ptr<int>)' <FunctionToPointerDecay>
// CHECK-NEXT:     DeclRefExpr {{.*}} 'int (_Ptr<int>, _Ptr<int>)' Function {{0x[0-9a-f]+}} 'cmp' 'int (_Ptr<int>, _Ptr<int>)'
}

typedef _Ptr<_For_any(S) void(_Array_ptr<S> p : count(len), int len, unsigned size, _Ptr<int (_Ptr<S>, _Ptr<S>)> cmp)> transform_type;

_For_any(T) void short_compose(_Array_ptr<T> p : count(len), int len, unsigned size, transform_type f1, transform_type f2, _Ptr<int (_Ptr<T>, _Ptr<T>)> cmp);

#if 0
// TODO: this won't compile properly yet.  There are multiple issues:
// 1. We don't handle type application of a pointer to a polymorphic function
//    These would be applications of f1 and f2
// 2. We don't allow (*f1) either.  The compiler IR doesn't support this.
// There is a 3rd issue not hit here, which is that use a top-level function within
// a generic function body results in type variables with the wrong depth level.
_For_any(T) void short_compose(_Array_ptr<T> p : count(len), int len, unsigned size, 
                               transform_type f1, transform_type f2, _Ptr<int(_Ptr<T>, _Ptr<T>)> cmp) {
   f1(p, len, size, cmp);
   f2(p, len, size, cmp);
}

#endif

// This illustrates how a pointer to a function can be used directly at a call.
// If we use generic types, this doesn't compile yet.
typedef void (*transform_type2)(void *p, int len, unsigned size, int (*cmp)(void *, void *));

int cmp2(void *a, void *b);

void void_compose(void *p, int len, unsigned size, transform_type2 f1, transform_type2 f2) {
  f1(p, len, size, cmp2);
  f2(p, len, size, cmp2);
}
