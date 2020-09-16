// RUN: cconv-standalone -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#include <stdlib.h>
#include <stddef.h>
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

struct tree {
  int val;
  struct tree *parent;
  struct tree **children;
  int len;
  int child_count;
};

//CHECK: _Ptr<struct tree> parent;
//CHECK_ALL: _Array_ptr<_Ptr<struct tree>> children : count(len);
//CHECK_NOALL: struct tree **children;

struct tree *new_node(int val, unsigned int num_childs, struct tree *parent) {
//CHECK_ALL: _Ptr<struct tree> new_node(int val, unsigned int num_childs, _Ptr<struct tree> parent) { 
//CHECK_NOALL: _Ptr<struct tree> new_node(int val, unsigned int num_childs, _Ptr<struct tree> parent) {

  struct tree *n = malloc(sizeof(struct tree));

  //CHECK_ALL: _Ptr<struct tree> n = malloc<struct tree>(sizeof(struct tree));
  //CHECK_NOALL: _Ptr<struct tree> n =  malloc<struct tree>(sizeof(struct tree));
  
  struct tree **children;

  //CHECK_ALL: _Array_ptr<_Ptr<struct tree>> children : count(num_childs) = ((void *)0); 
  //CHECK_NOALL: struct tree **children;
  if (!n) return NULL;
  children = malloc(sizeof(struct tree *)*num_childs);
  //FIX: use calloc instead
  n->val = val;
  n->parent = parent;
  n->len = num_childs;  
  n->children = children;
  n->child_count = 0;
  return n;
}

int add_child(struct tree *p, struct tree *c) {
//CHECK_ALL: int add_child(_Ptr<struct tree> p, _Ptr<struct tree> c) _Checked {
//CHECK_NOALL: int add_child(_Ptr<struct tree> p, struct tree *c) { 

  if (p->child_count >= p->len) {
    unsigned int len = p->len * 2;
    struct tree **children;
    //CHECK_ALL: _Array_ptr<_Ptr<struct tree>> children : count(len) = ((void *)0);
    //CHECK_NOALL: struct tree **children;

    //children = realloc(p->children,sizeof(struct tree)*len);
    //children = malloc(sizeof(struct tree)*len);
    p->children = children;
    if (!p->children) return -1;
    p->len = len;
  }
  p->children[p->child_count] = c;
  c->parent = p;
  p->child_count++;
  return 0;
}



int sum(struct tree *p) {
//CHECK_ALL: int sum(_Ptr<struct tree> p) _Checked {
//CHECK_NOALL: int sum(struct tree *p : itype(_Ptr<struct tree>)) {
  int n = 0;
  if (!p) return 0;
  n += p->val;
  for (int i = 0; i<p->child_count; i++) {
    n += sum(p->children[i]);
  }
  return n;
}    

