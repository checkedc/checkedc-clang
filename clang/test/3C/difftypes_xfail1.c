//RUN: rm -rf %t*
//RUN: 3c -base-dir=%S -output-dir=%t.checked %s %S/difftypes_xfail2.c --

// XFAIL: *

// The desired behavior in this case is to fail, so other checks are omitted

_Ptr<int> foo(int, char);
