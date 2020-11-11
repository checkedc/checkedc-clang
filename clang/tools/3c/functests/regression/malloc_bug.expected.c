#include<stdlib_checked.h>
int main(void) {
   _Ptr<int> p =  malloc(sizeof(int));
   int *q = malloc(sizeof(int));
   free(q);
   q = (int*)(0xDEADBEEF);
}
