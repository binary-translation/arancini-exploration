#include <immintrin.h>
__m128 test(__m128 a, __m128 b) {
    return _mm_div_round_ss(a, b, _MM_FROUND_CUR_DIRECTION);
}