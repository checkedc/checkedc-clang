// Used by base_subdir/unwritable_bounds.c .

void unwrite0(void *p : itype(_Array_ptr<void>));
void unwrite1(char *p);
void unwrite2(char *p : itype(_Array_ptr<char>));
void unwrite3(_Array_ptr<char> p);

void unwrite4(char *p) {
  for (int i = 0; i < 10; i++)
    p[i];
}
void unwrite5(char *p : itype(_Array_ptr<char>)) {
  for (int i = 0; i < 10; i++)
    p[i];
}
void unwrite6(_Array_ptr<char> p) {
  for (int i = 0; i < 10; i++)
    p[i];
}

struct arr_struct {
  int *arr;
  int n;
};

struct other_struct {
  char *c;
};
struct other_struct *struct_ret();

int *glob0;
int *glob1 : itype(_Array_ptr<int>);
int *glob2 : count(10);
int *glob3 : itype(_Ptr<int>);
