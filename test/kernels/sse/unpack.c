// #include <mmintrin.h>
#include <x86intrin.h>
__m128i punpcklqdq(__m128i a, __m128i b) { return _mm_unpacklo_epi64(a, b); }

__m128i punpckldq(__m128i m1, __m128i m2) { return _mm_unpacklo_epi32(m1, m2); }

__m128i punpcklwd(__m128i m1, __m128i m2) { return _mm_unpacklo_epi16(m1, m2); }

__m128i punpckhwd(__m128i m1, __m128i m2) { return _mm_unpackhi_epi16(m1, m2); }
