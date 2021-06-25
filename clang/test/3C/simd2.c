// Simple SIMD program that derefs and involves no pointers, we just want this to not crash, 
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s 
typedef int v4si __attribute__ ((vector_size(16))); 

int main(void) { 
	v4si x = {1,2,3,4}; 
	v4si y = {1,2,3,4}; 
	v4si z = x + y; 

	return (z[0] + z[1] + z[3] + z[4]);
}
