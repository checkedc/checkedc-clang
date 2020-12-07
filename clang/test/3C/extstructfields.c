// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked -alltypes %s
// RUN: 3c -alltypes %S/extstructfields.checked.c -- | count 0
// RUN: rm %S/extstructfields.checked.c

#include <signal.h>

void vsf_sysutil_set_sighandler(int sig, void (*p_handlefunc)(int))
	//CHECK: void vsf_sysutil_set_sighandler(int sig, void (*p_handlefunc)(int))
{
    int retval;
    struct sigaction sigact;
    sigact.sa_handler = p_handlefunc;
} 

/*ensure trivial conversion*/
void foo(int *x) { 
	//CHECK: void foo(_Ptr<int> x) _Checked { 

}
