#include <immintrin.h>
__m256 test(__m256 a) {
  return _mm256_sqrt_ps(a);
}
