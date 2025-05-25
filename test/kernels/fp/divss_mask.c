#include <immintrin.h>
__m128 test(__m128 s, __mmask8 k, __m128 a, __m128 b) {
    return _mm_mask_div_ss(s, k, a, b);
}