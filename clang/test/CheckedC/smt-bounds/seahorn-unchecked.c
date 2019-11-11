#include <stdio.h>
#include "seahorn/seahorn.h"

int foo(int *a : count(n), int n);

int foo(int *a : count(n), int n)
{
    assume( n > 0 );
    assume( a != NULL );

    int i = 0, s = 0;
    for(i=0; i<n; i++)
        s += a[i];

    return s;
}

