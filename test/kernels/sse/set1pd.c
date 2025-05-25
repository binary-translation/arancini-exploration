#include <emmintrin.h>
__m128d test(double a) { return _mm_set1_pd(a); }
