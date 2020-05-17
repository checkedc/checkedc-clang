#include <stdlib_checked.h>

int main() {

  _Ptr<char> ptr1 =  NULL;

  ptr1 = (char *) calloc(1, sizeof(char));

  return 0;
}
