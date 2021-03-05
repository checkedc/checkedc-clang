// RUN: rm -rf %t*
// RUN: not 3c -base-dir=%S -output-dir=%t.checked %s %S/multidef1a.c -- 2>%t.stderr
// RUN: grep -q "merging failed" %t.stderr

// The desired behavior in this case is to fail, so other checks are omitted

int * foo(int x, _Ptr<char> y) { 
    x = x + 4; 
    int *z = &x; 
    return z;
}
