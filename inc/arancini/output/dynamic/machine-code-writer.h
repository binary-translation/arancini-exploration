#pragma once

#include <cstdlib>

namespace arancini::output::dynamic {
class machine_code_writer {
public:
	machine_code_writer()
		: code_ptr_(nullptr)
		, code_size_(0)
		, alloc_size_(0)
	{
	}

	template <typename T> void emit(T v)
	{
		ensure_capacity(sizeof(T));
		((T *)code_ptr_)[code_size_] = v;
		code_size_ += sizeof(T);
	}

	void emit8(unsigned char c) { emit<decltype(c)>(c); }
	void emit16(unsigned short c) { emit<decltype(c)>(c); }
	void emit32(unsigned int c) { emit<decltype(c)>(c); }
	void emit64(unsigned long c) { emit<decltype(c)>(c); }

	void *ptr() const { return code_ptr_; }
	size_t size() const { return code_size_; }

	void finalise()
	{
		code_ptr_ = std::realloc(code_ptr_, code_size_);
		alloc_size_ = code_size_;
	}

private:
	void ensure_capacity(size_t extra)
	{
		if ((code_size_ + extra) > alloc_size_) {
			if (alloc_size_ == 0) {
				alloc_size_ = 64;
			} else {
				alloc_size_ *= 2;
			}

			code_ptr_ = std::realloc(code_ptr_, alloc_size_);
		}
	}

	void *code_ptr_;
	size_t code_size_;
	size_t alloc_size_;
};
} // namespace arancini::output::dynamic
