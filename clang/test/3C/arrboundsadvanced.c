// RUN: 3c -alltypes %s -- | FileCheck -match-full-lines %s


/*
Advanced array-bounds inference (based on control-dependencies).
*/

struct foo {
    void *data;
};
//CHECK:     void *data;
struct foo1 {
    int *x;
    // This is to make sure that length heuristic will not
    // kick in
    unsigned x_len;
    unsigned ml;    
};
//CHECK:     _Array_ptr<int> x : count(ml);
struct foo **FL;
int *intarr;
//CHECK: _Array_ptr<_Ptr<struct foo>> FL : count(FooLen) = ((void *)0);
//CHECK: _Array_ptr<int> intarr = ((void *)0);
unsigned FooLenD;
unsigned FooLen;
void intcopy(int *arr, int *ptr, int len) {
    int i;
    for (i=0; i<len; i++) {
        // This will make len the length of arr and ptr.
        arr[i] = ptr[i];
    }
}
//CHECK: void intcopy(_Array_ptr<int> arr : count(len), _Array_ptr<int> ptr : count(len), int len) {

void setdata(struct foo **G, unsigned dlen, struct foo *d, unsigned idx) {
    if (idx >= dlen) {
        return;
    }
    if (idx >= FooLenD) {
        // This is not a control-dependent node.
        printf("Default length more");
    }
    // This will make dlen the length of G
    G[idx] = d;
}
//CHECK: void setdata(_Array_ptr<_Ptr<struct foo>> G : count(dlen), unsigned dlen, _Ptr<struct foo> d, unsigned idx) {

int main(int argc, char **argv) {
    char *PN = argv[0];
    unsigned i = 3, n;
    struct foo1 po;
    setdata(FL, FooLen, 0, 0);
    n = po.ml;
    if ( i < n && i < FooLenD && i < FooLen) {
        // This will make ml the length of X
        po.x[i] = 0;
        // This will not make FooLenD or FooLen the size of intarr
        // because we don't know which one is the right length.
        intarr[i] = 0;
    }
    return 0;
}
//CHECK: int main(int argc, _Array_ptr<_Ptr<char>> argv : count(argc)) {
//CHECK:    _Ptr<char> PN =  argv[0];
//CHECK:    struct foo1 po = {};
