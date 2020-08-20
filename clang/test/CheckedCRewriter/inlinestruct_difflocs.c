// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: cconv-standalone -output-postfix=checked -alltypes %s
// RUN: cconv-standalone -alltypes %S/inlinestruct_difflocs.checked.c -- | count 0
// RUN: rm %S/inlinestruct_difflocs.checked.c

int valuable;

static struct foo
{
  const char* name;
	//CHECK_NOALL: const char* name;
	//CHECK_ALL:   _Ptr<const char> name;
  int* p_valuable;
	//CHECK: _Ptr<int> p_valuable;
}
array[] =
{
  { "mystery", &valuable }
}; 

