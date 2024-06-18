/**
Check function pointer bounds rewriting.
**/

// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -fcheckedc-extension -x c -o %t1.unusedl -
// RUN: 3c -base-dir=%S %s -- | FileCheck -match-full-lines -check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S %s -- | %clang -c -fcheckedc-extension -x c -o %t2.unused -

void (*fn)(_Array_ptr<char> buf : count(l), unsigned int l);
//CHECK: _Ptr<void (_Array_ptr<char> buf : count(l), unsigned int l)> fn = ((void *)0);
