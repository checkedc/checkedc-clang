// Test case for ArrayBound and ArrayBoundV2 checkers in static analyzer
// The simple case is caught by these checkers as out-of-bound access
// But the complex case (bitwise operation) is not caught

int bar(int *p, int idx)
{
    return p[idx];
}


int foo(int x) {
    int a[5];


    int i = 2;
    int j = i * 3;

    // in both cases there should be a bug report
    // but in the BITWISE case, the solver can't handle
    // the expression
    //
#ifdef BITWISE
    x &= 1;
    int k = j % (7 * (x | (x ^ 1))); // x is either 0 or 1 => k = 6
#else
    int k = j % 10;
#endif

    return bar(a, k);
}

