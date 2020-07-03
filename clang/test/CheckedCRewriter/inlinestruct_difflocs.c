// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -output-postfix=checkedNOALL %s
// RUN: %clang -c %S/inlinestruct_difflocs.checkedNOALL.c
// RUN: rm %S/inlinestruct_difflocs.checkedNOALL.c

int valuable;

static struct foo
{
  const char* name;
  int* p_valuable;
}
array[] =
{
  { "mystery", &valuable }
}; 

//CHECK_ALL: _Ptr<const char> name;
//CHECK_NOALL: const char* name;
//CHECK: _Ptr<int> p_valuable;