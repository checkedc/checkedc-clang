// This is a test case to check support for AST generation in
// the presence of invalid checked pointer usage.
// https://github.com/microsoft/checkedc-clang/pull/847#issuecomment-652375065
//
// This test checks that the compiler does not generate any error while trying
// to generate AST for programs even when the Checked C type checking fails.
//
// RUN: %clang -cc1 -verify -f3c-tool %s
// expected-no-diagnostics

struct st1 {
    _Array_ptr<int> q;
};

int bar(_Array_ptr<int> q : count(l), unsigned l) {
    unsigned i;
    for (i=0; i<l; i++) {
      q[i] = 0;
    }
    return 0;
}

_Array_ptr<int> baz(_Ptr<int> q, unsigned z) : count(5) {
    int *p;
    return p;
}

int zzz(int *o, unsigned l) {
    return 0;
}

void fpt(_Ptr<int(_Array_ptr<int> arr : count(i), int i)> j) {
    return;
}

int foo(void) {
    // No initializer.
   _Array_ptr<int> r : count(5);
   _Nt_array_ptr<char> z;
   _Array_ptr<_Ptr<int>> n;
   struct st1 o;
   char *l; 
   _Ptr<int> q;
   _Ptr<int(_Array_ptr<int> arr : count(i), int i)> j;
   int *p;
   void *v;
   // different types of assignments.
   q = p;
   r = p;
   p = r;
   q = p;
   r = p;
   l = z;
   z = l;
   r = z;
   n = p !=0 ? p : q;
   n = p;
   p = n;
   v = n;
   n = v;
   *n = v;
   *n = p;
   *n = q;
   *n = r;
   r = *n;
   o.q = p;
   n = o.q;
   // assigning ptr to array.
   p = baz(q, 0);
   // assigning unchecked pointer to checked ptr.
   p = baz(p, 0);
   // Function pointers.
   fpt(&bar);
   fpt(&zzz);
   fpt(&baz);
   j = baz;
   return 0;
}
