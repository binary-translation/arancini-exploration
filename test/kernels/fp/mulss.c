#include <immintrin.h>
__m128 test(__m128 a, __m128 b) {
  return _mm_mul_ss(a,b);
}
