// Since the RUN commands in difftypes1a.c and difftypes1b.c process the two
// files in different orders and the location where the error is reported
// depends on the order, we need to use a different diagnostic verification
// prefix (and set of corresponding comments) for each RUN command. Verification
// is per translation unit, so the translation unit with no diagnostic needs
// `expected-no-diagnostics`.

//RUN: rm -rf %t*
//RUN: 3c -base-dir=%S -output-dir=%t.checked %s %S/difftypes1b.c -- -Xclang -verify=ab-expected

// ab-expected-no-diagnostics
// ba-expected-error@+1 {{merging failed for 'foo'}}
_Ptr<int> foo(int, char);
