//
// These is a regression test case for
// https://github.com/Microsoft/checkedc-clang/issues/458
//
// This test checks that _Dynamic_bounds_cast from a void pointer compiles.
//
// RUN: %clang -cc1 -verify -fcheckedc-extension %s
// expected-no-diagnostics

typedef struct {
	void *ptr : itype(_Ptr<void>);
} mytype_t;

int
main(void)
{
	mytype_t m;
 (void)_Dynamic_bounds_cast<_Ptr<char>>(m.ptr);
}

