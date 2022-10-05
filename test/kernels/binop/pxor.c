#include <emmintrin.h>
__m128i test(__m128i a, __m128i b) {
  return _mm_xor_si128(a,b);
}
