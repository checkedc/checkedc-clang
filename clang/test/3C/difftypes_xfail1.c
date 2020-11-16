//RUN: 3c -base-dir=%S -output-postfix=checked %s %S/difftypes_xfail2.c
// XFAIL: * 

_Ptr<int> foo(int, char);