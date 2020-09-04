// XFAIL: *
// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s
// XFAIL: *

/*
Various array bounds tests.
*/

#include <stddef.h>
void *memset(void * dest : byte_count(n),
             int c,
             size_t n) : bounds(dest, (_Array_ptr<char>)dest + n);
_Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
_Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);

int *g; // array : count is 100; see the call to foo
void foo(void) {
  int *h = malloc(sizeof(int)*100); // length is 100
  g = h; // length is 100
  // make it an array
  g[0] = 0;
}
//CHECK: _Array_ptr<int> g : count(100) = ((void *)0); // array : count is 100; see the call to foo
//CHECK: _Array_ptr<int> h : count(100) =  malloc<int>(sizeof(int)*100); // length is 100


struct tree {
  int* vals; // array: count is 2
  struct tree **children; // array : count is num
  int num;
  struct tree *parent; // ptr
};
//CHECK: _Array_ptr<int> vals : count(2); // array: count is 2
//CHECK-NEXT: _Array_ptr<_Ptr<struct tree>> children : count(num); // array : count is num
//CHECK: _Ptr<struct tree> parent; // ptr

void add_child(struct tree *p) {
  struct tree *c = malloc(sizeof(struct tree));
  c->vals = malloc(sizeof(int)*2);
  c->vals[0] = 0;
  c->children = 0;
  c->num = 0;
  c->parent = p;
  int n = p->num+1;
  struct tree **xs = realloc(p->children,sizeof(struct tree *)*n);
  p->children = xs;
  p->num = n;
}
//CHECK: void add_child(_Ptr<struct tree> p) {
//CHECK-NEXT: _Ptr<struct tree> c =  malloc<struct tree>(sizeof(struct tree));
//CHECK: _Array_ptr<_Ptr<struct tree>> xs : count(n) = realloc<struct tree>(p->children,sizeof(struct tree *)*n);

void do_c(void (*fp)(char), char *cp) {
  while (*cp != '\0' && *cp != ' ') {
    fp(*cp);
    cp++;
  }
}
//CHECK: void do_c(_Ptr<void (char )> fp, _Array_ptr<char> cp) {

void main(int argc, char *argv[]) 
{
   --argc;
   char *tmp;
   if (argc != 0) {
      do {
	//printf("%s\n", *++argv);
        tmp = argv[argc-1];
      } while (--argc > 0);
   }
}
//CHECK: void main(int argc, _Array_ptr<_Ptr<char>> argv : count(argc))
//CHECK: _Ptr<char> tmp = ((void *)0);

struct arg { char *p; int n; };
struct arg args[] = {
  { "hello", 0 },
  { "there", 27 },
  { "bob", 14 }
}; 
//CHECK: struct arg { _Ptr<char> p; int n; };
//CHECK-NEXT: struct arg args _Checked[3] =  {

void proc(int *p) {
  while (*p != 0) {
    //printf ("%d\n",*p);
    p++;
  }
}
//CHECK: void proc(int *p : itype(_Array_ptr<int>)) {

void go(void) {
  int p[10];
  memset(p,0,10);
  p[0] = 1;
  p[1] = 2;
  proc(p);
}
