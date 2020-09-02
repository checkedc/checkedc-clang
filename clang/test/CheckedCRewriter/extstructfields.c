// UNSUPPORTED: system-windows
// RUN: CPATH=$CHECKED_CPATH:$CPATH cconv-standalone %s -- | FileCheck -match-full-lines %s
// RUN: CPATH=$CHECKED_CPATH:$CPATH cconv-standalone %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#include <signal.h>

void vsf_sysutil_set_sighandler(int sig, void (*p_handlefunc)(int))
{
    int retval;
    struct sigaction sigact;
    sigact.sa_handler = p_handlefunc;
} 
//CHECK: void vsf_sysutil_set_sighandler(int sig, void (*p_handlefunc)(int))

/*ensure trivial conversion*/
void foo(int *x) { 

}
//CHECK: void foo(_Ptr<int> x) {
