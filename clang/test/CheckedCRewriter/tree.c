// RUN: cconv-standalone -addcr -alltypes %s -- | FileCheck -match-full-lines %s

#include <stdlib.h>

struct tree {
  int val;
  struct tree *parent;
  struct tree **children;
  int len;
  int child_count;
};

//CHECK: _Ptr<struct tree> parent;
//CHECK: _Array_ptr<_Ptr<struct tree>> children : count(len);

struct tree *new_node(int val, unsigned int num_childs, struct tree *parent) {
  struct tree *n = malloc(sizeof(struct tree));
  struct tree **children;
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

//CHECK: _Ptr<struct tree> new_node(int val, unsigned int num_childs, _Ptr<struct tree> parent) {
//CHECK: _Ptr<struct tree> n = malloc<struct tree>(sizeof(struct tree));
//CHECK: _Array_ptr<_Ptr<struct tree>> children : count(num_childs) = ((void *)0);

int add_child(struct tree *p, struct tree *c) {
  if (p->child_count >= p->len) {
    unsigned int len = p->len * 2;
    struct tree **children;
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

//CHECK:int add_child(_Ptr<struct tree> p, _Ptr<struct tree> c) _Checked{
//CHECK:_Array_ptr<_Ptr<struct tree>> children : count(len) = ((void *)0);

int sum(struct tree *p) {
  int n = 0;
  if (!p) return 0;
  n += p->val;
  for (int i = 0; i<p->child_count; i++) {
    n += sum(p->children[i]);
  }
  return n;
}    

//CHECK:int sum(_Ptr<struct tree> p) _Checked{
