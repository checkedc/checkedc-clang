#include<stdlib_checked.h>
// parameter subtyping.
int using_as_ptr(char *str) {
   if(*str == 0) {
       return 1;
   }
   return 0;
}
int using_as_arr(char *str) {
  if(str[0] == 0) {
      return 1;
  }
  return 0;
}

int using_as_ntarr(char *nt) {
  nt = "hello";
  return 1;
}

int using_as_wild(char *nt) {
  nt = (char*)(0xdeadbeef);
  return 1;
}

// return subtyping
char *returning_nt_arr() {
  return "hello";
}

char* returning_arr() {
   char *st = (char*)malloc(2);
   st[0] = 's';
   st[1] = 0;
   return st;
}

char* returning_wild() {
   char *p = (char*)(0xdeadbeef);
   return p;
}

int main(void) {
   // this will be NTARR
   char *nt_str = "NTARR";

   // this will be an NTARR too
   char *ptr;
   char *ptr1;
   char *ptr2;
   // nothing happens here.
   using_as_ptr(nt_str);
   using_as_ptr(ptr);

   using_as_arr(nt_str);
   // this will make ptr and NTARR
   // because we are using ptr in the same
   // place as nt_str (which is an NTARR)
   using_as_arr(ptr);

   using_as_ntarr(nt_str);

   // nothing happens to ptr1
   using_as_wild(ptr1);
  
   // this doesn't change ptr1
   // because although we are returning
   // nt_arr, ptr1 is used as PTR in this function.
   ptr1 = returning_nt_arr();
   // this will make return value of 
   // returning_arr as NTARR
   ptr = returning_arr();

   ptr2 = returning_nt_arr();
   // this will make ptr2 wild
   // and the return value of returning_nt_arr
   // will become an itype.
   ptr2 = returning_wild();
   return 0;   
}
