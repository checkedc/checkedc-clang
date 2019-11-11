// Examples of false-positive bug report by static analyzer
// These bug reports can be refuted by Z3
//

//
// The condition (i % width == 0) is always true and "base" always gets a value.
// But the analyzer reports use of uninitialized of "base"
//


#include <assert.h>

void foo(unsigned width) {
    assert( width > 0 );
    int base;
    int i = 0;

    if ( i % width == 0 )
        base = 1;

    assert( base == 1 );
}

//
// The expression ((x & 1) && ((x & 1) ^ 1)) always evaluates to 0.
// This expression is not handled by internal solver of the analyzer
// and therefore both paths are considered, and in the _then_ branch
// a null dereference is reported.
//

int bar(int x)
{
    int *z = 0;
    if ((x & 1) && ((x & 1) ^ 1))
        return *z;
    return 0;
}

