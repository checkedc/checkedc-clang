//RUN: 3c -base-dir=%S -output-postfix=checked %s %S/multidef_xfail2.c
// XFAIL: *

_Ptr<int> foo(int x, char * y) { 
    x = x + 4; 
    int *z = &x; 
    return z;
}