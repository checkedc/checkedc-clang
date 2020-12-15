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

_Checked void f5(void) {}

_Checked _Bounds_only void f6(void) {}

_Unchecked void f7(void) {}

_Checked void f8(void) _Checked _Bounds_only {}