// Used by base_subdir/canwrite_constraints_unimplemented.c and
// stdout_mode_write_other.c .
//
// This file has a strange name so that
// base_subdir/canwrite_constraints_unimplemented.c can test that 3C compares
// entire path components and knows that base_subdir_partial_defn.h is not under
// base_subdir (see
// https://github.com/correctcomputation/checkedc-clang/issues/327).

// The lack of a return type here is intentional. The return type is in the file
// that includes base_subdir_partial_defn.h.
foo(int *x) {}
