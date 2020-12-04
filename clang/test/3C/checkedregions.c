// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked -alltypes %s
// RUN: 3c -alltypes %S/checkedregions.checked.c -- | count 0
// RUN: rm %S/checkedregions.checked.c
/* Tests for adding (un)checked regions automatically */

#include <stddef.h> 

int foo(int *x) { 
	//CHECK: int foo(_Ptr<int> x) _Checked { 
  return *x;
}

int bar(int *x) { 
	//CHECK: int bar(_Ptr<int> x) _Checked { 
  int i;
  for(i = 0; i<2; i++) { 
    *x = i;
  }
  return *x;
}


int gar(int *x) { 
	//CHECK: int gar(int *x) { 
  x = (int*) 4;
	//CHECK: x = (int*) 4;
  return *x;
}


int f(void) { 
  char* u = (char*) 3;
	//CHECK: char* u = (char*) 3;

  if(1) { 
	//CHECK: if(1) _Checked { 
    return 1;
  } else { 
	//CHECK: } else _Checked { 
    return 2;
  }
}


int faz(void) { 
	//CHECK: int faz(void) _Checked { 
  if(1) { 
	//CHECK: if(1) _Unchecked { 
    int *x = (int*) 3;
	//CHECK: int *x = (int*) 3;
    return *x;
  } 
  if(1) { 
	//CHECK: if(1) _Unchecked { 
    int *x = (int*) 3;
	//CHECK: int *x = (int*) 3;
    return *x;
  }
}


char* bad(void) { 
	//CHECK: char* bad(void) { 
  return (char*) 3;
	//CHECK: return (char*) 3;
}


void baz(void) { 
	//CHECK: void baz(void) _Checked { 
  int x = 3;
  if(x) { 
	//CHECK: if(x) _Unchecked { 
    bad();
  } else { 
	//CHECK: } else _Unchecked { 
    bad();
  }
}
