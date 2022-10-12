#include <emmintrin.h>
__m128 test(__m128 a, __m128 b) {
  return _mm_div_ps(a,b);
}
