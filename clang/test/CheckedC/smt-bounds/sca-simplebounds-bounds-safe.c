// Test case for SimpleBounds checker when there is bounds expression
// information available for the bounds checking.
// 

int foo(int *a : count(n + 2), int n);
int foo(int *a, int n)
{
    int k = n + n;
    int t = (k & 1) | ((k & 1) ^ 1);

    a[t - 1] = 1; // The index is always zero here, it should be ok
    a[n / 2] = 1; // This should be ok too
    a[k] = 1; // This should trigger a bug

    return 0;
}


