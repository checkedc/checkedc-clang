/**
Test for context sensitive bounds for internal functions.
**/

// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
_Itype_for_any(T) void *somefunc(unsigned long size) : itype(_Array_ptr<T>) byte_count(size);
struct hash_node
{
  int *p_key;
  int *q_key;
  int *r_key;
  int *w_key;
  int *y_key;
  unsigned pqlen;
  unsigned r_len;
  unsigned xo;
  unsigned lo;
};

//CHECK_ALL: _Array_ptr<int> p_key : byte_count(pqlen);
//CHECK_NOALL: int *p_key;
//CHECK_ALL: _Array_ptr<int> q_key : byte_count(pqlen);
//CHECK_NOALL: int *q_key;
//CHECK_ALL: _Array_ptr<int> r_key : byte_count(r_len);
//CHECK_NOALL: int *r_key;
//CHECK_ALL: _Array_ptr<int> w_key : count(xo);
//CHECK_NOALL: int *w_key;
//CHECK_ALL: _Array_ptr<int> y_key : count(lo);
//CHECK_NOALL: int *y_key;
  
int bar(struct hash_node *p) {
    p->p_key = p->q_key;
    return 0;
}
//CHECK_ALL: int bar(_Ptr<struct hash_node> p) {
//CHECK_NOALL: int bar(_Ptr<struct hash_node> p) {


void ctxsensfunc(int *p, unsigned n) {
    unsigned i;
    for (i=0; i<n; i++) {
	p[i] = 0;
    }
}
//CHECK_ALL: void ctxsensfunc(_Array_ptr<int> p : count(n), unsigned n) {
//CHECK_NOALL: void ctxsensfunc(int *p, unsigned n) {

int foo() {
    unsigned i,j;
    struct hash_node *n = somefunc(sizeof(struct hash_node));
    i = 5*sizeof(int);
    n->pqlen = i;
    n->p_key = somefunc(i);
    n->r_key = somefunc(n->r_len);
    ctxsensfunc(n->w_key, n->xo);
    n->p_key[0] = 1;
    n->r_key[0] = 1;
    j = n->lo;
    ctxsensfunc(n->y_key, j);
    return 0;
}
//CHECK_ALL: _Ptr<struct hash_node> n =  somefunc<struct hash_node>(sizeof(struct hash_node));
//CHECK_NOALL: struct hash_node *n =  somefunc<struct hash_node>(sizeof(struct hash_node));
