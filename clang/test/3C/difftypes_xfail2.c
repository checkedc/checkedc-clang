//RUN: 3c -base-dir=%S -output-postfix=checked %s %S/difftypes_xfail1.c
// XFAIL: *

int * foo(int, char *);
