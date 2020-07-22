typedef unsigned long size_t;
#define NULL 0
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));


struct np {
    int x;
    int y;
};

struct p {
    _Ptr<int> x;
    char *y;
};


struct r {
    int data;
    _Ptr<struct r> next;
};


_Ptr<struct p> sus(_Ptr<struct p> x, _Ptr<struct p> y) {
  x->y += 1;
  _Ptr<struct p> z =  malloc<struct p>(sizeof(struct p));
  return z;
}

_Ptr<struct p> foo(void) {
  int ex1 = 2, ex2 = 3;
 _Ptr<struct p> x = ((void *)0);
_Ptr<struct p> y = ((void *)0);
 
  x->x = &ex1;
  y->x = &ex2;
  x->y = &ex2;
  y->y = &ex1;
  _Ptr<struct p> z =  (struct p *) sus(x, y);
  return z;
}

_Ptr<struct p> bar(void) {
  int ex1 = 2, ex2 = 3;
 _Ptr<struct p> x = ((void *)0);
_Ptr<struct p> y = ((void *)0);
 
  x->x = &ex1;
  y->x = &ex2;
  x->y = &ex2;
  y->y = &ex1;
  _Ptr<struct p> z =  (struct p *) sus(x, y);
  return z;
}