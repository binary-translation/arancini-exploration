#include <immintrin.h>
#include <emmintrin.h>
__m256i test(__m256i a, __m256i b) {
  return _mm256_add_epi8(a,b);
}

