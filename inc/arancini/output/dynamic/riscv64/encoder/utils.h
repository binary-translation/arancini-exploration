// Copyright (c) 2012, the Newspeak project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#ifndef VM_UTILS_H_
#define VM_UTILS_H_

#include <arancini/output/dynamic/riscv64/encoder/assert.h>
#include <arancini/output/dynamic/riscv64/encoder/globals.h>

namespace arancini::output::dynamic::riscv64 {

class Utils {
public:
	template <typename T> static inline T RoundDown(T x, intptr_t n)
	{
		ASSERT(IsPowerOfTwo(n));
		return (x & -n);
	}

	template <typename T> static inline T RoundUp(T x, intptr_t n) { return RoundDown(x + n - 1, n); }

	static inline int HighestBit(int64_t v)
	{
		uint64_t x = static_cast<uint64_t>((v > 0) ? v : -v);
#if defined(__GNUC__)
		ASSERT(sizeof(long long) == sizeof(int64_t)); // NOLINT
		if (v == 0)
			return 0;
		return 63 - __builtin_clzll(x);
#else
		uint64_t t;
		int r = 0;
		if ((t = x >> 32) != 0) {
			x = t;
			r += 32;
		}
		if ((t = x >> 16) != 0) {
			x = t;
			r += 16;
		}
		if ((t = x >> 8) != 0) {
			x = t;
			r += 8;
		}
		if ((t = x >> 4) != 0) {
			x = t;
			r += 4;
		}
		if ((t = x >> 2) != 0) {
			x = t;
			r += 2;
		}
		if (x > 1)
			r += 1;
		return r;
#endif
	}

	static int BitLength(int64_t value)
	{
		// Flip bits if negative (-1 becomes 0).
		value ^= value >> (8 * sizeof(value) - 1);
		return (value == 0) ? 0 : (Utils::HighestBit(value) + 1);
	}

	template <typename T> static inline bool IsPowerOfTwo(T x) { return ((x & (x - 1)) == 0) && (x != 0); }

	template <typename T> static inline bool IsAligned(T x, intptr_t n)
	{
		ASSERT(IsPowerOfTwo(n));
		return (x & (n - 1)) == 0;
	}

	// Check whether an N-bit two's-complement representation can hold value.
	template <typename T> static inline bool IsInt(int N, T value)
	{
		ASSERT((0 < N) && (static_cast<unsigned int>(N) < (kBitsPerByte * sizeof(value))));
		T limit = static_cast<T>(1) << (N - 1);
		return (-limit <= value) && (value < limit);
	}

	template <typename T> static inline bool IsUint(int N, T value)
	{
		ASSERT((0 < N) && (static_cast<unsigned int>(N) < (kBitsPerByte * sizeof(value))));
		const auto limit = (static_cast<typename std::make_unsigned<T>::type>(1) << N) - 1;
		return (0 <= value) && (static_cast<typename std::make_unsigned<T>::type>(value) <= limit);
	}
};

} // namespace arancini::output::dynamic::riscv64

#endif // VM_UTILS_H_
