// RUN: rm -rf %t
// RUN: mkdir %t && cd %t
// RUN: python -c 'import sys, json; json.dump([{"arguments": ["clang", "-c", "%s"], "directory": "%S", "file": "%s"}]*2, sys.stdout)' > compile_commands.json
// RUN: 3c -dump-stats -p %t -base-dir=%S %s | FileCheck -match-full-lines %s
// RUN: python -c 'import sys, json; exit(any(e["Name"].startswith("ImplicitCastExpr") for e in json.load(sys.stdin)["RootCauseStats"]))' < PerWildPtrStats.json

// The compilation database used for this test includes two entries for this
// file, causing 3C to process it twice. In issue #661, this caused type
// argument instantiation to fail.

// This issue also made an erroneous entry appear in the root cause output. The
// root cause did not appear in the -warn-root-cause warnings. It was only
// present in the root cause statistics json output. The json is used to
// generate the output for 3c-wrap root_cause, so the error appeared there as well.

// Furthermore, in stdout mode, if the main file was processed twice, the
// rewritten version was printed to stdout twice
// (https://github.com/correctcomputation/checkedc-clang/issues/374#issuecomment-893612654).
// Check that this doesn't happen any more.

_Itype_for_any(T) void my_free(void *pointer : itype(_Array_ptr<T>) byte_count(0));

void foo() {
  //CHECK: {{^}}void foo() {
  int *a;
  my_free(a);
  //CHECK: my_free<int>(a);
}

// Make sure the file does not get printed to stdout a second time. Since
// -match-full-lines does not apply to CHECK-NOT, the {{^}} is needed to anchor
// the match to the beginning of the line and prevent the CHECK-NOT from
// matching itself.
//CHECK-NOT: {{^}}void foo() {
