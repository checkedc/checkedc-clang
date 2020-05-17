//
// This example is from https://github.com/Microsoft/checkedc-clang/issues/319
//
// RUN: %clang -fcheckedc-extension %s -o %t
// RUN: %t | FileCheck %s

#include <stdio_checked.h>
#include <stdchecked.h>

int sum(int *a, int n);
int sum(int *a : count(n), int n);

int main(int argc, char **argv)
{
    int n, i, a[20];

    for( i=0; i<20; i++ )
        a[i] = -1000;

    n = 10;
    for( i=0; i<n; i++ )
        a[i] = i + 1;

    array_ptr<int> chk_a : count(n) = a;

    // CHECK: unchecked : 55
    printf("unchecked : %d\n", sum(a, n));

    // CHECK: checked : 55
    printf("checked : %d\n", sum(chk_a, n));

    return 0;
}

int sum(int *a, int n)
{
    int i, s = 0;

    for( i=0; i<n; i++ )
    {
        s += a[i];
    }

    return s;
}
