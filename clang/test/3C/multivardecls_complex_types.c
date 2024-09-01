// A few tests of multi-decls with complex types, with and without
// -itypes-for-extern. These tests cannot be included in multivardecls.c because
// compiling that entire file with -itypes-for-extern -addcr would produce an
// unrelated error in the typedef multi-decl test: see the first example in
// https://github.com/correctcomputation/checkedc-clang/issues/740.

// RUN: rm -rf %t*

// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/multivardecls_complex_types.c -- | diff %t.checked/multivardecls_complex_types.c -

// RUN: 3c -base-dir=%S -itypes-for-extern -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ITE_ALL","CHECK_ITE" %s
// RUN: 3c -base-dir=%S -itypes-for-extern -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ITE_NOALL","CHECK_ITE" %s
// RUN: 3c -base-dir=%S -itypes-for-extern -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -itypes-for-extern -alltypes -output-dir=%t.checked_ITE %s --
// RUN: 3c -base-dir=%t.checked_ITE -itypes-for-extern -alltypes %t.checked_ITE/multivardecls_complex_types.c -- | diff %t.checked_ITE/multivardecls_complex_types.c -

// A multi-decl with a mix of pointers and arrays, including an "unchecked
// pointer to constant size array" member that would trigger a known bug in
// mkString (item 5 of
// https://github.com/correctcomputation/checkedc-clang/issues/703),
// demonstrating that unchanged multi-decl members whose base type wasn't
// renamed use Decl::print (which doesn't have this bug).
//
// `m_implicit_itype` and `m_change_with_bounds` together test that 3C includes
// Checked C annotations in the range to be replaced (for both changed and
// unchanged multi-decl members) rather than leaving them to duplicate the
// annotations in the newly inserted declaration.
int m_const_arr0[10], *m_const_arr1[10], (*m_p_const_arr_wild)[10] = 1,
    (*m_p_const_arr_chk)[10], *m_implicit_itype : count(2),
    **m_change_with_bounds : count(2);
//CHECK_ALL:   int m_const_arr0 _Checked[10];
//CHECK_NOALL: int m_const_arr0[10];
//CHECK_ALL:   _Ptr<int> m_const_arr1 _Checked[10] = {((void *)0)};
// The reason this isn't `_Ptr<int> m_const_arr1[10]` is probably the "outer
// wild -> inner wild" constraint
// (https://github.com/correctcomputation/checkedc-clang/issues/656).
//CHECK_NOALL: int *m_const_arr1[10];
//CHECK:       int (*m_p_const_arr_wild)[10] = 1;
//CHECK_ALL:   _Ptr<int _Checked[10]> m_p_const_arr_chk = ((void *)0);
//CHECK_NOALL: _Ptr<int[10]> m_p_const_arr_chk = ((void *)0);
// 3C doesn't have proper support for itypes on variables: if a variable has an
// existing itype, 3C uses the checked side as the variable's original type. So
// 3C treats m_implicit_itype as having original type _Array_ptr<int>, but since
// the solved type is the same, 3C uses Decl::print for the unchanged multi-decl
// member and preserves the original declaration with the itype. When 3C gains
// proper itype support for variables, it should generate an actual rewrite to
// the fully checked type if nothing else in the program prevents it from doing
// so.
//CHECK:       int *m_implicit_itype : count(2);
// In this case, the solved type changes and shows up in the output.
//CHECK:       _Array_ptr<_Ptr<int>> m_change_with_bounds : count(2) = ((void *)0);

// Test the same multi-decl with -itypes-for-extern.
//CHECK_ITE_ALL:   int m_const_arr0[10] : itype(int _Checked[10]);
//CHECK_ITE_NOALL: int m_const_arr0[10];
//CHECK_ITE_ALL:   int *m_const_arr1[10] : itype(_Ptr<int> _Checked[10]) = {((void *)0)};
//CHECK_ITE_NOALL: int *m_const_arr1[10];
//CHECK_ITE:       int (*m_p_const_arr_wild)[10] = 1;
//CHECK_ITE_ALL:   int (*m_p_const_arr_chk)[10] : itype(_Ptr<int _Checked[10]>) = ((void *)0);
//CHECK_ITE_NOALL: int (*m_p_const_arr_chk)[10] : itype(_Ptr<int[10]>) = ((void *)0);
//CHECK_ITE:       int *m_implicit_itype : count(2);
//CHECK_ITE:       int **m_change_with_bounds : itype(_Array_ptr<_Ptr<int>>) count(2) = ((void *)0);

// A similar multi-decl with an unnamed inline struct, which forces the use of
// mkString. We can't include (*s_p_const_arr_*)[10] because it would trigger
// the previously mentioned mkString bug and produce output that doesn't
// compile. `s` serves just to give the struct a shorter generated name.
struct { int *x; } s, s_const_arr0[10], *s_const_arr1[10],
    // The only way a variable of unnamed struct type can have an itype is if it
    // comes implicitly from a bounds annotation, since we have no name to refer
    // to the struct in a written itype.
    *s_implicit_itype : count(2), **s_change_with_bounds : count(2);
//CHECK:       struct s_struct_1 { _Ptr<int> x; };
//CHECK:       struct s_struct_1 s;
//CHECK_ALL:   struct s_struct_1 s_const_arr0 _Checked[10];
//CHECK_NOALL: struct s_struct_1 s_const_arr0[10];
//CHECK_ALL:   _Ptr<struct s_struct_1> s_const_arr1 _Checked[10] = {((void *)0)};
//CHECK_NOALL: struct s_struct_1 *s_const_arr1[10];
// Like with m_implicit_itype above, 3C treats s_implicit_itype as having type
// _Array_ptr<struct s_struct_1>, but now 3C uses mkString and that type
// actually shows up in the output: not a great result, but at least we test
// that it isn't any worse and that the bounds annotation is preserved. Since
// s_implicit_itype is now the only member of its "multi-decl", if the user
// manually edits it back to an itype, there won't be another multi-decl breakup
// to cause 3C to mess it up again.
//CHECK:       _Array_ptr<struct s_struct_1> s_implicit_itype : count(2);
//CHECK:       _Array_ptr<_Ptr<struct s_struct_1>> s_change_with_bounds : count(2) = ((void *)0);

// Test the same multi-decl with -itypes-for-extern.
//CHECK_ITE:       struct s_struct_1 { int *x : itype(_Ptr<int>); };
//CHECK_ITE:       struct s_struct_1 s;
//CHECK_ITE_ALL:   struct s_struct_1 s_const_arr0[10] : itype(struct s_struct_1 _Checked[10]);
//CHECK_ITE_NOALL: struct s_struct_1 s_const_arr0[10];
//CHECK_ITE_ALL:   struct s_struct_1 *s_const_arr1[10] : itype(_Ptr<struct s_struct_1> _Checked[10]) = {((void *)0)};
//CHECK_ITE_NOALL: struct s_struct_1 *s_const_arr1[10];
// The type of s_implicit_type is still loaded as _Array_ptr<struct s_struct_1>,
// but it is downgraded back to an itype by -itypes-for-extern. As long as 3C
// lacks real support for itypes on variables, this is probably the behavior we
// want with -itypes-for-extern in this very unusual case.
//CHECK_ITE:       struct s_struct_1 *s_implicit_itype : itype(_Array_ptr<struct s_struct_1>) count(2);
//CHECK_ITE:       struct s_struct_1 **s_change_with_bounds : itype(_Array_ptr<_Ptr<struct s_struct_1>>) count(2) = ((void *)0);
