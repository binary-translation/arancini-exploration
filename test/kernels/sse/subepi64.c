#include <emmintrin.h>
__m128i test(__m128i a, __m128i b) {
  return _mm_sub_epi64(a,b);
}
