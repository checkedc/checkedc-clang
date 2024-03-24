// Since the RUN commands in difftypes2a.c and difftypes2b.c process the two
// files in different orders and the location where the error is reported
// depends on the order, we need to use a different diagnostic verification
// prefix (and set of corresponding comments) for each RUN command. Verification
// is per translation unit, so the translation unit with no diagnostic needs
// `expected-no-diagnostics`.

// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -output-dir=%t.checked %s %S/difftypes2b.c -- -Xclang -verify=ab-expected

// The desired behavior in this case is to fail, so other checks are omitted

// Test no body vs body

// ab-expected-no-diagnostics
// ba-expected-error@+1 {{merging failed for 'foo'}}
void foo(char *x);
