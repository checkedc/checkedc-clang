// RUN: cconv-standalone -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -alltypes -output-postfix=checked %s
// RUN: cconv-standalone -alltypes %S/definedType.checked.c -- | diff %S/definedType.checked.c -
// RUN: rm %S/definedType.checked.c

#define size_t unsigned long
_Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

// From issue 204

#define	ulong	unsigned long

ulong *		TOP;
// CHECK_NOALL: ulong *		TOP;
// CHECK_ALL: _Array_ptr<ulong> TOP = ((void *)0);
ulong			channelColumns;

void
DescribeChannel(void)
{
    ulong	col;
    TOP = (ulong *)malloc((channelColumns+1) * sizeof(ulong));
    // CHECK_ALL: TOP = (_Array_ptr<unsigned long>)malloc<unsigned long>((channelColumns+1) * sizeof(ulong));
    // CHECK_NOALL: TOP = (ulong *)malloc<unsigned long>((channelColumns+1) * sizeof(ulong));
    TOP[col] = 0;
}


#define integer int
integer foo(int *p, int l) {
// CHECK_ALL: integer foo(_Array_ptr<int> p : count(l), int l)  _Checked {
// CHECK_NOALL: integer foo(int *p, int l) {
   return p[l-1];
}

int *bar(integer p, integer i) {
// CHECK: _Ptr<int> bar(integer p, integer i) _Checked {
  return 0;
}

// Macros containing only the base type are kept in checked pointer

#define baz unsigned int

baz a;
// CHECK: baz a;
baz *b;
// CHECK: _Ptr<baz> b = ((void *)0);
baz **c;
// CHECK: _Ptr<_Ptr<baz>> c = ((void *)0);
baz d[1];
// CHECK_ALL: baz d _Checked[1];
baz *e[1];
// CHECK_ALL: _Ptr<baz> e _Checked[1];
baz **f[1];
// CHECK_ALL: _Ptr<_Ptr<baz>> f _Checked[1];
baz (*g)[1];
// CHECK_ALL: _Ptr<baz _Checked[1]> g = ((void *)0);
baz h[1][1];
// CHECK_ALL: baz h _Checked[1] _Checked[1];

baz *i(){
// CHECK: _Ptr<baz> i(void)_Checked {
  return 0;
}

baz **j(){
// CHECK: _Ptr<_Ptr<baz>> j(void)_Checked {
  return 0;
}

void k(baz x, baz *y, baz **z) {}
// COM: void k(baz x, _Ptr<baz> y, _Ptr<_Ptr<baz>> z) _Checked {}

// Macros are inlined if there's a pointer in the macro
// This could probably be handled better in the future.

#define buz int*

buz l;
// CHECK: _Ptr<int> l = ((void *)0);

buz *m;
// CHECK: _Ptr<_Ptr<int>> m = ((void *)0);

// Macro should not change when wild

buz n = (buz) 1;
// CHECK: buz n = (buz) 1;
