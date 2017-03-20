// Tests for the Checked C rewriter tool.

extern void bar(int *q : itype(_Ptr<int>));

void foo(int *p : itype(_Ptr<int>)) {
	*p = 0;
	return;
}
