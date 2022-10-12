#include <immintrin.h>
__m256 test(float a) {
  return _mm256_set1_ps(a);
}
