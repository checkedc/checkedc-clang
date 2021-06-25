// Simple SIMD program that involves no pointers, we just want this to not crash
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s 

typedef int v4si __attribute__ ((vector_size(16))); 

void main(void) { 
	v4si x = {1,2,3,4}; 
	v4si y = {1,2,3,4}; 
	v4si z = x + y; 

}
