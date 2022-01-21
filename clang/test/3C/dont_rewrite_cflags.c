// This tests a case of unwritable files being written under specific flags
// to the compiler, as seen in the run command below. It was originally noticed
// converting a header file to use generic variables. The solution is to check
// again that your change is in a writable file, as seen here:
// https://github.com/correctcomputation/checkedc-clang/commit/b6d8ed1dcfc9d4174dc87b1b34a5ff6def3f2d5c

// RUN: 3c -base-dir=%S %s -- -O2 -D_FORTIFY_SOURCE=2

#include <sys/socket.h>

