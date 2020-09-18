// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#include <stdarg.h>
typedef int lua_State;
extern void lua_lock(lua_State *);
extern void luaC_checkGC(lua_State *);
extern void lua_unlock(lua_State *);
extern const char *luaO_pushvfstring (lua_State *L, const char *fmt, va_list argp);
const char *lua_pushfstring (lua_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
//CHECK: const char * lua_pushfstring(lua_State *L, const char *fmt, ...) {
//CHECK-NEXT:  const char *ret;
//CHECK-NEXT:  va_list argp;
  lua_lock(L);
  va_start(argp, fmt);
  ret = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  luaC_checkGC(L);
  lua_unlock(L);
  return ret;
}
