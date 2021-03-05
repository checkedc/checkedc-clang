// RUN: rm -rf %t*
// RUN: not 3c -base-dir=%S -output-dir=%t.checked %s %S/multidef1b.c -- 2>%t.stderr
// RUN: grep "merging failed" %t.stderr

// The desired behavior in this case is to fail, so other checks are omitted

_Ptr<int> foo(int x, char * y) { 
    x = x + 4; 
    int *z = &x; 
    return z;
}
