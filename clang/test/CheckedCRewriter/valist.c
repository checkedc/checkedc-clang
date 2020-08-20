// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/valist.checked.c -- | count 0
// RUN: rm %S/valist.checked.c

#include <stdarg.h>
typedef int lua_State;
extern void lua_lock(lua_State *);
extern void luaC_checkGC(lua_State *);
extern void lua_unlock(lua_State *);
extern const char *luaO_pushvfstring (lua_State *L, const char *fmt, va_list argp);
const char *lua_pushfstring (lua_State *L, const char *fmt, ...) {
	//CHECK: const char *lua_pushfstring (lua_State *L, const char *fmt, ...) {
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
/*force output*/
int *p;
	//CHECK: _Ptr<int> p = ((void *)0);
