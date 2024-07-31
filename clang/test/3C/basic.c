// RUN: 3c -base-dir=%S -alltypes %s -- -Wno-error=implicit-function-declaration | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S %s -- -Wno-error=implicit-function-declaration | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S %s -- -Wno-error=implicit-function-declaration | %clang -c -Wno-error=implicit-function-declaration -fcheckedc-extension -x c -o /dev/null -
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stddef.h>

void basic1() {
  char data[] = "abcdefghijklmnop";

  char *buffer = malloc(50);
  strcpy(buffer, data);

  free(buffer);
  free(buffer); // Double free
}

//CHECK_NOALL: char data[] = "abcdefghijklmnop";
//CHECK_ALL: char data _Nt_checked[] =  "abcdefghijklmnop";
//CHECK: char *buffer = malloc<char>(50);

char *basic2(int temp) {
  char data[] = "abcdefghijklmnop";
  char data2[] =
      "abcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnop";

  if (temp) {
    char *buffer = malloc(8);
    strcpy(buffer, data2); // Overflow
    if (temp) {
      free(buffer);
    } else {
      puts(buffer);
    }
    return buffer; // Return after free
  } else {
    char *buffer = malloc(1024);
    strcpy(buffer, data); // Data not freed
    return 0;
  }
}
//CHECK_ALL: char *basic2(int temp) : itype(_Nt_array_ptr<char>) {
//CHECK_NOALL: char *basic2(int temp) : itype(_Ptr<char>) {
//CHECK_ALL: char data _Nt_checked[] =  "abcdefghijklmnop";
//CHECK_ALL: char data2 _Nt_checked[] =
//CHECK: char *buffer = malloc<char>(8);
//CHECK: char *buffer = malloc<char>(1024);

char *basic3(int *data, int count) {
  while (count > 1) {
    int *temp = malloc(sizeof(int));
    data = malloc(sizeof(int));
    count--;
  }
  return NULL;
}
//CHECK: _Ptr<char> basic3(_Ptr<int> data, int count) {
//CHECK: _Ptr<int> temp =  malloc<int>(sizeof(int));

void sum_numbers(int count) {
  int n, i, sum = 0;

  printf("Enter number of elements: ");
  n = count;

  int *ptr = (int *)malloc(n * sizeof(int));

  if (ptr == NULL) {
    printf("Error! memory not allocated.");
    exit(0);
  }
  printf("Enter elements: ");

  for (i = 0; i < n; ++i) {
    scanf("%d", ptr + i);
    sum += *(ptr + i);
  }
  free(ptr);
}
//CHECK_NOALL: int *ptr = (int *)malloc<int>(n * sizeof(int));
//CHECK_ALL: _Array_ptr<int> ptr : count(n) = (_Array_ptr<int>)malloc<int>(n * sizeof(int));

void basic_calloc(int count) {
  int n, i, sum = 0;

  printf("Enter number of elements: ");
  n = count;
  int *ptr = (int *)calloc(n, sizeof(int));

  if (ptr == NULL) {
    printf("Error! memory not allocated.");
    exit(0);
  }

  printf("Enter elements: ");

  for (i = 0; i < n; ++i) {
    scanf("%d", ptr + i);
    sum += *(ptr + i);
  }

  printf("Sum = %d", sum);
  free(ptr);
}
//CHECK_NOALL: int *ptr = (int *)calloc<int>(n, sizeof(int));
//CHECK_ALL: _Array_ptr<int> ptr : count(n) = (_Array_ptr<int>)calloc<int>(n, sizeof(int));

void basic_realloc(int count) {
  int i, n1, n2;

  printf("Enter size: ");

  n1 = count;
  int *ptr = (int *)malloc(n1 * sizeof(int));

  printf("Addresses of previously allocated memory: ");

  for (i = 0; i < n1; ++i)
    printf("%u\n", *(ptr + i));
  printf("\nEnter the new size: ");
  scanf("%d", &n2);
  ptr = realloc(ptr, n2 * sizeof(int));
  printf("Addresses of newly allocated memory: ");

  for (i = 0; i < n2; ++i)

    printf("%u\n", *(ptr + i));

  free(ptr);
}
//CHECK_NOALL: int *ptr = (int *)malloc<int>(n1 * sizeof(int));
//CHECK_ALL: _Array_ptr<int> ptr = (_Array_ptr<int>)malloc<int>(n1 * sizeof(int));

struct student {
  char name[30];
  int roll;
  float perc;
};
//CHECK: char name[30];

void basic_struct(int count) {

  int n, i;

  printf("Enter total number of elements: ");
  count = n;
  struct student *pstd = (struct student *)malloc(n * sizeof(struct student));

  if (pstd == NULL) {
    printf("Insufficient Memory, Exiting... \n");
  }

  for (i = 0; i < n; i++) {
    printf("\nEnter detail of student [%3d]:\n", i + 1);
    printf("Enter name: ");
    scanf(" "); /*clear input buffer*/
    gets((pstd + i)->name);
    printf("Enter roll number: ");
    scanf("%d", &(pstd + i)->roll);
    printf("Enter percentage: ");
    scanf("%f", &(pstd + i)->perc);
  }

  printf("\nEntered details are:\n");

  for (i = 0; i < n; i++) {
    printf("%30s \t %5d \t %.2f\n", (pstd + i)->name, (pstd + i)->roll,
           (pstd + i)->perc);
  }
}
//CHECK_NOALL: struct student *pstd = (struct student *)malloc<struct student>(n * sizeof(struct student));
//CHECK_ALL:  _Array_ptr<struct student> pstd : count(n) = (_Array_ptr<struct student>)malloc<struct student>(n * sizeof(struct student));

struct student *new_student() {
  char name[] = "Bilbo Baggins";
  struct student *new_s = malloc(sizeof(struct student));
  strcpy(new_s->name, name);
  new_s->roll = 3;
  new_s->perc = 4.3f;
  return NULL;
}
//CHECK: _Ptr<struct student> new_student(void) {
//CHECK_NOALL: char name[] = "Bilbo Baggins";
//CHECK_ALL: char name _Nt_checked[] =  "Bilbo Baggins";
//CHECK: _Ptr<struct student> new_s =  malloc<struct student>(sizeof(struct student));

int main() {
  basic1();
  basic2(1);
  basic2(2);
  sum_numbers(10);
  basic_calloc(100);
  basic_realloc(100);
  basic_struct(100);
}
