#include <emmintrin.h>
__m128 test(float a, float b, float c, float d) {
    return _mm_set_ps(a, b, c, d);
}
