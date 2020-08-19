// RUN: cconv-standalone -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#define size_t unsigned long
_Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

// From issue 204

#define	ulong	unsigned long

ulong *		TOP;
// CHECK_NOALL: ulong *		TOP;
// CHECK_ALL: _Array_ptr<unsigned long> TOP = ((void *)0);
ulong			channelColumns;

void
DescribeChannel(void)
{
    ulong	col;
    TOP = (ulong *)malloc((channelColumns+1) * sizeof(ulong));
    // CHECK_ALL: TOP = (_Array_ptr<unsigned long> )malloc<unsigned long>((channelColumns+1) * sizeof(ulong));
    // CHECK_NOALL: TOP = (ulong *)malloc<unsigned long>((channelColumns+1) * sizeof(ulong));
    TOP[col] = 0;
}

int *a = 0;
// CHECK: _Ptr<int> a = 0;
