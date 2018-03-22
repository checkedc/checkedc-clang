//
// These is a regression test case for
// https://github.com/Microsoft/checkedc-clang/issues/458
//
// This test checks that _Dynamic_bounds_cast from a void pointer compiles.
//
// RUN: %clang -fcheckedc-extension -c -o%t %s
// expected-no-diagnostics

typedef struct {
	void *ptr : itype(_Ptr<void>);
} mytype_t;

int main(int argc, char *argv[])
{
	mytype_t m;
  (void)_Dynamic_bounds_cast<_Ptr<char>>(m.ptr);

  return 0;
}

