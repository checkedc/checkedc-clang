// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/extstructfields.c -- | diff %t.checked/extstructfields.c -

// This regression test is for a 3C bug that affected vsftpd, which only builds
// on Linux. Windows does not have `struct sigaction`, and we couldn't find a
// reasonable way to write an analogous test that works on Windows.
// UNSUPPORTED: system-windows

#include <signal.h>

void vsf_sysutil_set_sighandler(int sig, void (*p_handlefunc)(int))
//CHECK: void vsf_sysutil_set_sighandler(int sig, void ((*p_handlefunc)(int)) : itype(_Ptr<void (int)>))
{
  int retval;
  struct sigaction sigact;
  sigact.sa_handler = p_handlefunc;
}

/*ensure trivial conversion*/
void foo(int *x) {
  //CHECK: void foo(_Ptr<int> x) _Checked {
}
