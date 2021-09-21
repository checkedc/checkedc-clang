// Tests for 3C.
//
// Checks to make sure _Nt_arrrays only contain pointers & integers
//
// RUN: 3c -alltypes -base-dir=%S %s -- | %clang -c  -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S %s -- | %clang -c  -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -alltypes -base-dir=%S %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// expected-no-diagnostics
struct h;

void k(struct h *e) {
  //CHECK_NOALL: void k(struct h *e : itype(_Ptr<struct h>)) {
  //CHECK_ALL: void k(struct h *e) {
  e = "";
}

void (*a)(struct h *) = k;
//CHECK_NOALL: _Ptr<void (struct h * : itype(_Ptr<struct h>))> a = k;
//CHECK_ALL: _Ptr<void (struct h *)> a = k;

void l(struct h *f) {
  //CHECK_NOALL: void l(_Ptr<struct h> f) {
  //CHECK_ALL: void l(struct h *f) {
  k(f);
}

struct foo { int i; };

struct foo* bar(int x) { 
  //CHECK: _Ptr<struct foo> bar(int x) {
  return 0;
}

int car(struct foo* ptr) { 
  //CHECK: int car(_Ptr<struct foo> ptr) {
  return ptr->i;
}

struct foobar { float* ptr; };
//CHECK: struct foobar { _Ptr<float> ptr; };
struct barfoo { float* ptr; };
//CHECK: struct barfoo { _Ptr<float> ptr; };

float* dar(struct foobar f) { 
  //CHECK: _Ptr<float> dar(struct foobar f) {
  return f.ptr;
}

// Test a bug I found in canBeNtArray triggered by some specific types.
// The P >= ARR constraint was incorrectly added to these variables, causing
// them to solve to WILD instead of NTARR.

void force_nt_arr(char *s : itype(_Nt_array_ptr<char>));

void decayed_type(char s[10]) {
//CHECK_NOALL: void decayed_type(char s[10]) {
//CHECK_ALL: void decayed_type(char s _Nt_checked[10]) {
  force_nt_arr(s);
}

void paren_type(char *(s)) {
//CHECK_NOALL: void paren_type(char *(s) : itype(_Ptr<char>)) {
//CHECK_ALL: void paren_type(_Nt_array_ptr<char> s) {
  force_nt_arr(s);
}

void decayed_paren_type(char (s)[10]) {
//CHECK_NOALL: void decayed_paren_type(char (s)[10]) {
//CHECK_ALL: void decayed_paren_type(char s _Nt_checked[10]) {
  force_nt_arr(s);
}
