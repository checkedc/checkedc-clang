// When -output-dir is in use, 3C calls essentially
// llvm::sys::path::replace_path_prefix(InputFilePath, BaseDir, OutputDir) to
// calculate the output file path. In at least some cases, replace_path_prefix
// just does a string replacement of BaseDir with OutputDir at the beginning of
// InputFilePath, so if BaseDir ends in a directory separator and OutputDir
// doesn't, the separator will get lost and a bogus output path will be
// generated. (In Checked C's LLVM 9 baseline (llvmorg-10-init^ =
// c89a3d78f43d81b9cff7b9248772ddf14d21b749), this happens only when BaseDir and
// OutputDir are exactly the same length. On the LLVM main branch as of this
// writing (9e62c9146d2c125a1abda594add70ed66008e372), code inspection suggests
// that it happens all the time.)
//
// 3C canonicalizes all paths with llvm::sys::fs::real_path (mainly for other
// reasons), and we hope that canonicalization will defend us against the above
// problem too by removing any trailing separator from BaseDir and/or OutputDir.
// But we have a dedicated test for this just in case the LLVM behavior changes.
// We choose the output dir %t/outxx to be exactly the same length as the base
// dir %t/base/ (including the separator). If the bug occurs, we'd get the bogus
// output path %t/outxxbase_dir_trailing_slash.c.

// RUN: rm -rf %t*
// RUN: mkdir -p %t/base
// RUN: cp %s %t/base
// RUN: 3c -base-dir=%t/base/ -output-dir=%t/outxx %t/base/base_dir_trailing_slash.c --
// RUN: FileCheck -match-full-lines --input-file %t/outxx/base_dir_trailing_slash.c %s
// RUN: test ! -f %t/outxxbase_dir_trailing_slash.c

// CHECK: _Ptr<int> p = ((void *)0);
int *p;
