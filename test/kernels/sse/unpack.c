//#include <mmintrin.h>
#include <x86intrin.h>
__m128i test(__m128i a, __m128i b) {
  return _mm_unpacklo_epi64(a, b);
}
