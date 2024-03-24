// Test AST generation for structs with generic itypes (itype_for_any).
//
// RUN: %clang_cc1 -ast-dump -fcheckedc-extension %s | FileCheck %s

// CHECK: TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced T '(0, 0) __BoundsInterface'
// CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 0) __BoundsInterface'
struct List _Itype_for_any(T) {
  void *head : itype(_Ptr<T>);
  struct List *next : itype(_Ptr<struct List<T> >);

  // The RecordDecl corresponding to the generic definition.

    // CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} List_Itype_for_any(1) definition
    // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} head 'void *'
    // CHECK-NEXT: InteropTypeExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<T>'
    // CHECK-NEXT: PointerType {{0x[0-9a-f]+}} '_Ptr<T>'
    // CHECK-NEXT: TypedefType {{0x[0-9a-f]+}} 'T' sugar
    // CHECK-NEXT: Typedef {{0x[0-9a-f]+}} 'T'
    // CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 0) __BoundsInterface'
    // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} next 'struct List <void>*'
    // CHECK-NEXT: InteropTypeExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<struct List<T>>'
    // CHECK-NEXT: PointerType {{0x[0-9a-f]+}} '_Ptr<struct List<T>>'
    // CHECK-NEXT: ElaboratedType {{0x[0-9a-f]+}} 'struct List<T>' sugar
    // CHECK-NEXT: RecordType {{0x[0-9a-f]+}} 'struct List<T>'
    // CHECK-NEXT: Record {{0x[0-9a-f]+}} 'List'

    // Notice that by declaring 'next' to be of type 'struct List', we actually generated
    // a type application 'struct List<void>', so we need to create a decl for 'List<void>'.

    // CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct List 'struct List<void>' definition
    // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} head 'void *'
    // CHECK-NEXT: InteropTypeExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<void>'
    // CHECK-NEXT: PointerType {{0x[0-9a-f]+}} '_Ptr<void>'
    // CHECK-NEXT: BuiltinType {{0x[0-9a-f]+}} 'void'
    // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} next 'struct List <void>*'
    // CHECK-NEXT: InteropTypeExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<struct List<void>>'
    // CHECK-NEXT: PointerType {{0x[0-9a-f]+}} '_Ptr<struct List<void>>'
    // CHECK-NEXT: ElaboratedType {{0x[0-9a-f]+}} 'struct List<void>' sugar
    // CHECK-NEXT: RecordType {{0x[0-9a-f]+}} 'struct List<void>'
    // CHECK-NEXT: Record {{0x[0-9a-f]+}} 'List'

    // Additionally, in the type itype we used a 'struct List<T>', so we need to create
    // that definition as well.

    // CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct List 'struct List<T>' definition
    // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} <invalid sloc> head 'void *'
    // CHECK-NEXT: InteropTypeExpr {{0x[0-9a-f]+}} {{.*}} '_Ptr<T>'
    // CHECK-NEXT: PointerType {{0x[0-9a-f]+}} '_Ptr<T>'
    // CHECK-NEXT: TypedefType {{0x[0-9a-f]+}} 'T' sugar
    // CHECK-NEXT: Typedef {{0x[0-9a-f]+}} 'T'
    // CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 0) __BoundsInterface'
    // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} next 'struct List <void>*'
    // CHECK-NEXT: InteropTypeExpr {{0x[0-9a-f]+}} <<invalid sloc>> '_Ptr<struct List<T>>'
    // CHECK-NEXT: PointerType {{0x[0-9a-f]+}} '_Ptr<struct List<T>>'
    // CHECK-NEXT: ElaboratedType {{0x[0-9a-f]+}} 'struct List<T>' sugar
    // CHECK-NEXT: RecordType {{0x[0-9a-f]+}} 'struct List<T>'
    // CHECK-NEXT: Record {{0x[0-9a-f]+}} 'List'
};
