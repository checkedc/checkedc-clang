#include <stdio_checked.h>
#include <stdlib_checked.h>
#include <stdchecked.h>
#include "seahorn/seahorn.h"

int foo(array_ptr<int> p : count(n), int n)
{
    assume( n > 0 );
    assume( p != NULL );
    array_ptr<int> a : count(n) = p;

    a[n / 2] = 1;

    int i = 0, s = 0;
    for(i=0; i<n; i++)
        s += a[i];

    return s;
}


#if 0
int main(int argc, nt_array_ptr<char> argv checked[] : count(argc)) {

    int n = argc > 1 ? atoi(argv[1]) : 10;
    array_ptr<int> ma : count(n) = malloc(n * sizeof(int));
    assume( ma != NULL );

    return foo(ma, n);
}
#endif
