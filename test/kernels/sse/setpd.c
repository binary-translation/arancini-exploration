#include <emmintrin.h>
__m128d test(double a, double b) { return _mm_set_pd(a, b); }
