// Regression test for a null dereference in
// AvarBoundsInference::getReachableBoundKeys
// (https://github.com/correctcomputation/checkedc-clang/issues/523). This
// example was reduced from ImageMagick. We haven't analyzed the conditions that
// trigger the crash in order to construct a targeted test that we are more
// confident would catch a regression even if other changes are made to 3C, but
// this is better than nothing.

// We only care that this doesn't crash.
// RUN: 3c -base-dir=%S -alltypes %s --

int a;
void b(int *f, int g) {
  int *c = f;
  (void)f[a];
}
void (*h)(int *, int) = b;
