// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone --addcr --alltypes %s -- | %clang_cc1  -fcheckedc-extension -x c -
//
// cconv-standalone -alltypes -output-postfix=checked %s
// cconv-standalone -alltypes %S/typedefs.checked.c -- | diff %S/typedefs.checked.c -
// rm %S/typedefs.checked.c

typedef char* string;
//CHECK_ALL: typedef _Array_ptr<char> string;

void foo(void) {
	string x = "hello";
	//CHECK_ALL: string x : byte_count(5) = "hello";
	
	char c = x[2];

}
