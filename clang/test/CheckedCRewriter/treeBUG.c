// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// XFAIL: *

#define NULL 0
typedef unsigned long size_t;
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *memcpy(void * restrict dest : itype(restrict _Array_ptr<T>) byte_count(n),
             const void * restrict src : itype(restrict _Array_ptr<const T>) byte_count(n),
             size_t n) : itype(_Array_ptr<T>) byte_count(n);

struct tree {
  int val;
  struct tree *parent;
//CHECK: _Ptr<struct tree> parent;
  struct tree **children;
// FIX_CHECK_ALL: _Array_ptr<_Ptr<struct tree>> children : count(len);
//CHECK_NOALL: struct tree **children;
  int len;
  int child_count;
};

struct tree *new_node(int val, unsigned int num_childs, struct tree *parent) {
//CHECK: _Ptr<struct tree> new_node(int val, unsigned int num_childs, _Ptr<struct tree> parent) { 
  struct tree *n = malloc(sizeof(struct tree));
  //CHECK: _Ptr<struct tree> n =  malloc<struct tree>(sizeof(struct tree));
  
  struct tree **children;
  //CHECK_ALL: _Array_ptr<_Ptr<struct tree>> children : count(num_childs) = ((void *)0); 
  //CHECK_NOALL: struct tree **children;
  if (!n) return NULL;
  children = malloc(num_childs*sizeof(struct tree *));
  //CHECK_ALL: children = malloc<_Ptr<struct tree>>(num_childs*sizeof(struct tree *));
  //CHECK_NOALL: children = malloc<struct tree *>(num_childs*sizeof(struct tree *));
  n->val = val;
  n->parent = parent;
  n->len = num_childs;  
  n->children = children;
  n->child_count = 0;
  return n;
}

int add_child(struct tree *p, struct tree *c) {
//CHECK_ALL: int add_child(_Ptr<struct tree> p, _Ptr<struct tree> c) {
//CHECK_NOALL: int add_child(_Ptr<struct tree> p, struct tree *c) { 
  if (p->child_count >= p->len) {
    unsigned int len = p->len * 2;
    struct tree **children = malloc(len*sizeof(struct tree *));
    //CHECK_ALL: _Array_ptr<_Ptr<struct tree>> children : count(len) = malloc<_Ptr<struct tree>>(len*sizeof(struct tree *));
    //CHECK_NOALL: struct tree **children = malloc<struct tree *>(len*sizeof(struct tree *));
    if (!children) return -1;
    memcpy(children,p->children,sizeof(struct tree *)*p->len);
    //CHECK_NOALL: memcpy<struct tree *>(children,p->children,sizeof(struct tree *)*p->len);
    //CHECK_ALL: memcpy<_Ptr<struct tree>>(children,p->children,sizeof(struct tree *)*p->len);
    free(p->children);
    //CHECK_NOALL: free<struct tree *>(p->children);
    //CHECK_ALL: free<_Ptr<struct tree>>(p->children);
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
//CHECK_ALL: int sum(_Ptr<struct tree> p) {
//CHECK_NOALL: int sum(struct tree *p : itype(_Ptr<struct tree>)) {
  int n = 0;
  if (!p) return 0;
  n += p->val;
  for (int i = 0; i<p->child_count; i++) {
    n += sum(p->children[i]);
  }
  return n;
}    

