// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S --addcr --alltypes %s -- | %clang -c  -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/typedefbounds.c -- | diff %t.checked/typedefbounds.c -

typedef char *string;
//CHECK_ALL: typedef _Array_ptr<char> string;

void foo(void) {
  string x = "hello";
  //CHECK_ALL: string x : byte_count(5) = "hello";

  char c = x[2];
}
