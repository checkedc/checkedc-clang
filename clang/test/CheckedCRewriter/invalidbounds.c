// RUN: cconv-standalone -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -


/*
Regression test for the issue: https://github.com/plum-umd/checkedc-clang/issues/239
Here, array bounds should be invalidated when conflicting bounds are assigned.
*/

#include <stddef.h>

extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
unsigned long strlen(const char *s : itype(_Nt_array_ptr<const char>)) ;

int foo() {
    const char* invalstr = "%b %d %H:%M";
    const char* valstr = "%b %d %H";
    //CHECK_ALL: _Nt_array_ptr<const char> invalstr = "%b %d %H:%M";
    //CHECK_NOALL: const char* invalstr = "%b %d %H:%M";
    //CHECK_ALL: _Nt_array_ptr<const char> valstr : byte_count(8) = "%b %d %H";
    //CHECK_NOALL: const char* valstr = "%b %d %H";

    unsigned n,k;
    char *arr1inval = malloc(n*sizeof(char));
    //CHECK_ALL: _Array_ptr<char> arr1inval = malloc<char>(n*sizeof(char));
    //CHECK_NOALL: char *arr1inval =  malloc<char>(n*sizeof(char));
    if (n > 0) {
    //CHECK_ALL: if (n > 0) _Checked {
    //CHECK_NOALL: if (n > 0) {
        invalstr = "%b %d %H";
    } else {
    //CHECK_ALL: } else _Checked {
    //CHECK_NOALL: } else {
        valstr = "%b %d %M";
    }
    strlen(invalstr);
    strlen(valstr);
   
    if (n > 3) {
	arr1inval = malloc(k*sizeof(char));
        //CHECK: arr1inval = malloc<char>(k*sizeof(char));
    }
    arr1inval[0] = 'a';
    return 0;
}
