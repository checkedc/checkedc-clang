// Original minimized failing simd program
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s 

__attribute__((__vector_size__(2 * sizeof(long)))) int a() {
  return (__attribute__((__vector_size__(2 * sizeof(long))))int){};
}
