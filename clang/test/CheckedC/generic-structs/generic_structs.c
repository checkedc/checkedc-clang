// Test AST generation for generic structs
//
// RUN: %clang_cc1 -ast-dump -fcheckedc-extension %s | FileCheck %s

// Just like for generic functions, a generic struct 'struct Foo _For_any(T)'' is
// desugared into first a 'typedef T new_type_variable' followed by a regular 'struct Foo'.

// CHECK: TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced T '(0, 0)'
// CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 0)'
struct List _For_any(T) {
  T *head;
  struct List<T> *tail;

  // Because we re-use 'RecordDecl' for both generic struct definitions and
  // instantiations of those definitions (type applications), a definition like the one
  // above for list generates two 'RecordDecls'.

  // The first RecordDecl represents the generic struct itself.

  // CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct List _For_any(1) definition
  // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} head 'T *'
  // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} 'struct List <T>*'

  // The second one corresponds to the type application 'struct List<T>' triggered
  // by the 'tail' field. This is a 'dummy' application that is only accessible within
  // the definition of 'List' itself, because it uses 'T' as an argument, and 'T'
  // is only in scope within the body of 'List'. Which is to say, this is a dummy application
  // that is needed for typechecking but can't be used from the outside.

  // CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct List 'struct List<T>' definition
  // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} head 'T *'
  // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} 'struct List <T>*'
};

// Next, we instantiate 'List<int>', which creates an additional 'List<int>' RecordDecl.
// Notice how the field type have changed to reflect the type substitution 'T -> int'. 

// CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct List 'struct List<int>' definition
// CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} head 'int *'
// CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} 'struct List <int>*'

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} li 'struct List <int>*'
struct List<int> *li;

// We declare a new variable of type 'List<int>'. Notice how the RecordDecl is re-used
// from the previous 'List<int>' variable. That is, we cache the results of type applications
// and for each application we create exactly one RecordDecl. This prevents cycles and is
// needed for correctness.

// CHECK-NEXT: VarDecl {{0x[0-9a-f]+}} {{.*}} li2 'struct List <int>*'
struct List<int> *li2;

// By contrast, now we declare a variable of type 'List<char>'. Since we haven't seen
// this particular type application before, we need to create a new RecordDecl.

// CHECK-NEXT: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct List 'struct List<char>' definition
// CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} head 'char *'
// CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} 'struct List <char>*'

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} lc 'struct List <char>*'
struct List<char> *lc;


// The next line shows how type applications can be nested.

// CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct List 'struct List<struct List<int>>' definition
// CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} head 'struct List <int>*'
// CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} tail 'struct List <struct List<int>>*'

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} lli 'struct List<struct List<int>>':'struct List<struct List<int>>'
struct List<struct List<int> > lli;

// The next test case defines a generic struct with two type parameters.
// Furthermore, the application appearing inside the definition switches the order
// of the parameters in the arguments. This is an example of 'polymorphic-recursion',
// meaning in this case that in the definition of 'AltList _For_any(U, V)' there are
// applications that are _not_ of the form 'AltList<U, V>' (in this case, there's an application
// 'AltList<U, V>').

// CHECK: TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced U '(0, 0)'
// CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 0)'
// CHECK-NEXT: TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced V '(0, 1)'
// CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 1)'
struct AltList _For_any(U, V) {
  U *head;
  struct AltList<V, U> *tail;

  // As before, we first see a RecordDecl for the generic struct itself.

  // CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct AltList _For_any(2) definition
  // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} head 'U *'
  // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} tail 'struct AltList <V, U>*'

  // Then we see a RecordDecl corresponding to 'AltList<V, U>'.

  // CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct AltList 'struct AltList<V, U>' definition
  // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} head 'V *'
  // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} tail 'struct AltList <U, V>*'

  // In turn, this generates a RecordDecl for 'AltList<U, V>'. The recursion is stopped
  // at this point, because we've already created and cached an 'AltList<V, U>'.

  // CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct AltList 'struct AltList<U, V>' definition
  // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} head 'U *'
  // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} tail 'struct AltList <V, U>*'
};

// We now instantiate 'AltList<void, int>'. Because we swap the type arguments in the body
// of 'AltList', this generates both 'AltList<void, int>' and 'AltList<int, void>'.

// CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct AltList 'struct AltList<void, int>' definition
// CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} head 'void *'
// CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} tail 'struct AltList <int, void>*'

// CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct AltList 'struct AltList<int, void>' definition
// CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} head 'int *'
// CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} tail 'struct AltList <void, int>*'

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} al 'struct AltList <void, int>*'
struct AltList<void, int> *al;

// In the next test case, we define two mutually-recursive generic structs.
// To create the cycle, we first forward-declare one of the structs.

// CHECK: TypedefDecl {{0x[0-9a-f]+}} {{.*}} col:24 B '(0, 0)'
// CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 0)'
// CHECK-NEXT: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct CycleB _For_any(1)
struct CycleB _For_any(B);

// CHECK: TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced A '(0, 0)'
// CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 0)'
struct CycleA _For_any(A) {
  struct CycleB<A> *next;

  // The generic RecordDecl is created first.

  // CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct CycleA _For_any(1) definition
  // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} next 'struct CycleB <A>*'

  // Then the application CycleB<A>.

  // CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct CycleB 'struct CycleB<A>' definition

  // TODO: fix the check below once caching of type applications works properly.
  // The check below should be FieldDecl {{0x[0-9a-f]+}} {{.*}} next 'struct CycleA <A>*'
  // The problem is that A and B both have level 0, so because of how we incorrectly cache
  // things we misremember the name.
  // (checkedc issue #661)
  // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} next 'struct CycleA <B>*'

  // Notice that we can't create CycleA<A>, because we don't know what's in the body
  // of CycleB<A> yet. We need to wait until CycleB is defined.
};

// CHECK: TypedefDecl {{0x[0-9a-f]+}} {{.*}} referenced B '(0, 0)'
// CHECK-NEXT: TypeVariableType {{0x[0-9a-f]+}} '(0, 0)'
struct CycleB _For_any(B) {
  struct CycleA<B> *next;

  // CHECK:RecordDecl {{0x[0-9a-f]+}} {{.*}} struct CycleB _For_any(1) definition
  // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} next 'struct CycleA <B>*'

  // CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct CycleA 'struct CycleA<B>' definition

  // TODO: fix the check below once caching of type applications works properly.
  // The check below should be FieldDecl {{0x[0-9a-f]+}} {{.*}} next 'struct CycleB <B>*'
  // The problem is that A and B both have level 0, so because of how we incorrectly cache
  // things we misremember the name.
  // (checkedc issue #661)
  // CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} next 'struct CycleB <A>*'

  // TODO: re-enable the checks below once the caching issue is resolved (checkedc issues #661).
  // Notice how after we've created 'CycleA<B>', we can go on to create 'CycleB<B>', since
  // we already know what's in the body of 'CycleA'.

  // TODO: re-enable with CHECK RecordDecl {{0x[0-9a-f]+}} {{.*}} struct CycleB 'struct CycleB<B>' definition
  // TODO: re-enable with CHECK-NEXT FieldDecl {{0x[0-9a-f]+}} {{.*}} next 'struct CycleA <B>*'
};

// TODO: re-enable the checks below once the caching issue is resolved (checkedc issues #661).
// At this point, since 'CycleB' is defined, we can 'complete' the RecordDecl 'CycleB<A>' created
// before. In turn, this makes us create a 'CycleA<A>' (because of the 'next' field in 'CycleB').

// TODO: re-enable with CHECK RecordDecl {{0x[0-9a-f]+}} {{.*}} struct CycleA 'struct CycleA<A>' definition
// TODO: re-enable with CHECK-NEXT FieldDecl {{0x[0-9a-f]+}} {{.*}} next 'struct CycleB <A>*'

// Next we instantiate 'CycleA<int>', which as expected creates both that type and 'CycleB<int>'.

// CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct CycleA 'struct CycleA<int>' definition
// CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} next 'struct CycleB <int>*'

// CHECK: RecordDecl {{0x[0-9a-f]+}} {{.*}} struct CycleB 'struct CycleB<int>' definition
// CHECK-NEXT: FieldDecl {{0x[0-9a-f]+}} {{.*}} next 'struct CycleA <int>*'

// CHECK: VarDecl {{0x[0-9a-f]+}} {{.*}} cycle 'struct CycleA <int>*'
struct CycleA<int> *cycle;
// Again, check that we cache things properly by not creating new RecordDecls.
// CHECK-NEXT: VarDecl {{0x[0-9a-f]+}} {{.*}} cycle2 'struct CycleA <int>*'
struct CycleA<int> *cycle2;