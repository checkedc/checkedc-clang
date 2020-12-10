// RUN: 3c -base-dir=%S -output-postfix=checked %s %S/multidef_xfail2.c

// XFAIL: *

// The desired behavior in this case is to fail, so other checks are omitted

_Ptr<int> foo(int x, char * y) { 
    x = x + 4; 
    int *z = &x; 
    return z;
}
