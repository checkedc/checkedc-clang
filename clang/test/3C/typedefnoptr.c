//RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
//RUN: 3c --addcr %s -- | %clang_cc1  -fcheckedc-extension -x c -

typedef unsigned int uint_t;
typedef uint_t *ptr_uint_t; 
//CHECK: typedef _Ptr<uint_t> ptr_uint_t;
void foo(void) {
	  ptr_uint_t x = 0;
	  //CHECK: ptr_uint_t x = 0;
}
