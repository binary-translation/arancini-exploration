#include <immintrin.h>
__m256d test(__m256d a, __m256d b) { return _mm256_div_pd(a, b); }
