// Tests for Checked C rewriter tool.
//
// Checks wrong array heuristics.
//
//RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
//RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s

int *glob;
int lenplusone;
typedef unsigned long size_t;
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
//CHECK_ALL: _Array_ptr<int> glob = ((void *)0);
//CHECK_NOALL: int *glob;

void foo(int *p, int idx) {
    p[idx] = 0;
}
//CHECK_ALL: void foo(_Array_ptr<int> p, int idx) {
//CHECK_NOALL: void foo(int *p, int idx) {

void bar(int *p, int flag) {
   if (flag&0x2) {
      p[0] = 0;
   }
}
//CHECK_ALL: void bar(_Array_ptr<int> p, int flag) {
//CHECK_NOALL: void bar(int *p, int flag) {

int gl() {
    int len;
    for(len=lenplusone; len >=1; len--) {
       glob[len] = 0;
    }
    return 0;
}

int deflen() {
    glob = malloc((lenplusone+1)*sizeof(int));    	
    return 0;
}
//CHECK: glob = malloc<int>((lenplusone+1)*sizeof(int));
