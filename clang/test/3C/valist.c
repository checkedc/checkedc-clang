// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/valist.c -- | diff %t.checked/valist.c -

#include <stdarg.h>
typedef int lua_State;
extern void lua_lock(lua_State *);
extern void luaC_checkGC(lua_State *);
extern void lua_unlock(lua_State *);
extern const char *luaO_pushvfstring(lua_State *L, const char *fmt,
                                     va_list argp);
const char *lua_pushfstring(lua_State *L, const char *fmt, ...) {
  //CHECK: const char *lua_pushfstring(lua_State *L : itype(_Ptr<lua_State>), const char *fmt : itype(_Ptr<const char>), ...) : itype(_Ptr<const char>) {
  const char *ret;
  //CHECK: const char *ret;
  va_list argp;
  lua_lock(L);
  va_start(argp, fmt);
  ret = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  luaC_checkGC(L);
  lua_unlock(L);
  return ret;
}

void foo(int i, ...) {
  va_list ap;
  va_start(ap, i);
  char *c = (char *)va_arg(ap, char *);
  //CHECK: char *c = (char *)va_arg(ap, char *);
  va_end(ap);
}

// Test for issue 484. va_list should not be expanded to anything else.
// Expanding it to struct __va_list_tag * causes compiler error on libtiff.
// Expanding to __builtin_va_list doesn't cause an error, but va_list is still
// preferable.

void bar(va_list y, int *z) {}
//CHECK: void bar(va_list y, _Ptr<int> z) {}
void (*baz)(va_list, int *);
//CHECK: _Ptr<void (va_list, _Ptr<int>)> baz = ((void *)0);
typedef void (*fiz)(va_list, int *);
//CHECK: typedef _Ptr<void (va_list, _Ptr<int>)> fiz;
typedef void fuz(va_list, int *);
//CHECK: typedef void fuz(va_list, _Ptr<int>);

/*force output*/
int *p;
//CHECK: _Ptr<int> p = ((void *)0);
