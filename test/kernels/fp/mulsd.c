#include <immintrin.h>
__m128d test(__m128d a, __m128d b) {
  return _mm_mul_sd(a,b);
}
