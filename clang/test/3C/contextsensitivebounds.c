/**
Test for context sensitive bounds.
**/

// RUN: 3c -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
_Itype_for_any(T) void *somefunc(unsigned long size) : itype(_Array_ptr<T>) byte_count(size);
struct hash_node
{
  int *p_key;
  int *q_key;
  int *r_key;
  unsigned pqlen;
  unsigned r_len;
};

//CHECK_ALL: _Array_ptr<int> p_key : byte_count(pqlen);
//CHECK_NOALL: int *p_key;
//CHECK_ALL: _Array_ptr<int> q_key : byte_count(pqlen);
//CHECK_NOALL: int *q_key;
//CHECK_ALL: _Array_ptr<int> r_key : byte_count(r_len);
//CHECK_NOALL: int *r_key;
  
int bar(struct hash_node *p) {
    p->p_key = p->q_key;
    return 0;
}
//CHECK_ALL: int bar(_Ptr<struct hash_node> p) {
//CHECK_NOALL: int bar(_Ptr<struct hash_node> p) {

int foo() {
    unsigned i;
    struct hash_node *n = somefunc(sizeof(struct hash_node));
    i = 5*sizeof(int);
    n->pqlen = i;
    n->p_key = somefunc(i);
    n->r_key = somefunc(n->r_len);
    n->p_key[0] = 1;
    n->r_key[0] = 1;
    return 0;
}
//CHECK_ALL: _Ptr<struct hash_node> n =  somefunc<struct hash_node>(sizeof(struct hash_node));
//CHECK_NOALL: struct hash_node *n =  somefunc<struct hash_node>(sizeof(struct hash_node));
