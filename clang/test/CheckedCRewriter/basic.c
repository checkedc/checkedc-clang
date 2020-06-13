// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s

#define NULL ((void*)0)
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));

void basic1() {
	char data[] = "abcdefghijklmnop";

	char *buffer = malloc(50);
	strcpy(buffer, data);

	free(buffer);
	free(buffer); // Double free
}

//CHECK: char data[] = "abcdefghijklmnop";
//CHECK: char *buffer = malloc(50);
//CHECK-NEXT: strcpy(buffer, data);

char* basic2(int temp) {
	char data[] = "abcdefghijklmnop";
	char data2[] = "abcdefghijklmnopabcdefghijklmnopabcdefghijklmnopabcdefghijklmnop";

	if (temp) {
		char *buffer = malloc(8);
		strcpy(buffer, data2); // Overflow
		if (temp) {
			free(buffer);
		}
		return buffer; // Return after free
	} else {
		char *buffer = malloc(1024);
		strcpy(buffer, data); // Data not freed
		return 0;
	}
}
//CHECK: char * basic2(int temp) {
//CHECK: char *buffer = malloc(8);
//CHECK: char *buffer = malloc(1024);

char* basic3(int* data, int count) {
	while (count > 1) {
		int* temp = malloc(sizeof(int));
		data = malloc(sizeof(int));
		count--;
	}
	return NULL;
}
//CHECK: _Ptr<char> basic3(_Ptr<int> data, int count) {
//CHECK: _Ptr<int> temp =  malloc(sizeof(int));

void sum_numbers(int count) {
    int n, i, sum = 0;

    printf("Enter number of elements: ");
	n = count;

    int *ptr = (int*) malloc(n * sizeof(int));

    if(ptr == NULL)
    {
        printf("Error! memory not allocated.");
    }
    free(ptr);
}
//CHECK: int *ptr = (int*) malloc(n * sizeof(int));


void basic_calloc(int count) {
    int n, i, sum = 0;

    printf("Enter number of elements: ");
	n = count;
    int *ptr = (int*) calloc(n, sizeof(int));

    if(ptr == NULL)
    {
        printf("Error! memory not allocated.");
    }

    printf("Enter elements: ");

    printf("Sum = %d", sum);
    free(ptr);
}
//CHECK: int *ptr = (int*) calloc(n, sizeof(int));

void basic_realloc(int count) {
    int i , n1, n2;

    printf("Enter size: ");

	n1 = count;
    int *ptr = (int*) malloc(n1 * sizeof(int));

    free(ptr);
}
//CHECK: int *ptr = (int*) malloc(n1 * sizeof(int));

struct student
{
    char name[30];
    int roll;
    float perc;
};
//CHECK: char name[30];

void basic_struct(int count) {

    int n,i;

    printf("Enter total number of elements: ");
	count = n;
    struct student *pstd=(struct student*)malloc(n*sizeof(struct student));



    if(pstd==NULL)
    {
        printf("Insufficient Memory, Exiting... \n");
    }

}
//CHECK: _Ptr<struct student> pstd = (struct student*)malloc(n*sizeof(struct student));

struct student * new_student() {
		char name[] = "Bilbo Baggins";
		struct student *new_s = malloc(sizeof(struct student));
		strcpy(new_s->name, name);
		new_s->roll = 3;
		new_s->perc = 4.3f;
		return NULL;
}
//CHECK: _Ptr<struct student> new_student(void) {
//CHECK-NEXT: char name[] = "Bilbo Baggins";
//CHECK: _Ptr<struct student> new_s =  malloc(sizeof(struct student));

int main() {
	basic1();
	basic2(1);
	basic2(2);
	sum_numbers(10);
	basic_calloc(100);
	basic_realloc(100);
	basic_struct(100);
}
