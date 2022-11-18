#include <emmintrin.h>

__m128i paddd_sse( __m128i a, __m128i b) {
    return _mm_add_epi32(a, b);
}

__m64 paddd_64(__m64 m1, __m64 m2) {
    return _mm_add_pi32(m1, m2);
}