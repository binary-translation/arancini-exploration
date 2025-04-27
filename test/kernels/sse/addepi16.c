#include <emmintrin.h>
__m128i test(__m128i a, __m128i b) { return _mm_add_epi16(a, b); }
