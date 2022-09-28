#include <immintrin.h>
__m256d test(double a) {
  return _mm256_set1_pd(a);
}
