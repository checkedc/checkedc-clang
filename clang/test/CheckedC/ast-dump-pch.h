// Used with the ast-dump-pch.c test

void f1(void) {
  _Checked { int a = 0; }
  _Checked _Bounds_only {int b = 0; }
  _Unchecked {}
  {}
}

void f2(void) _Checked {}

void f3(void) _Checked _Bounds_only {}

void f4(void) _Unchecked {}

// TODO: GitHub issue #704: add function declarations with checked scope specifiers