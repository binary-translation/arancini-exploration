#include <immintrin.h>

__m128 cvtsd2ss(__m128 a, __m128d b) { return _mm_cvtsd_ss(a, b); }
