#pragma once

#include <arancini/output/dynamic/machine-code-allocator.h>
#include <arancini/util/logger.h>
#include <cstring>
#include <iostream>

namespace arancini::output::dynamic {
class machine_code_writer {
public:
	machine_code_writer(machine_code_allocator &allocator)
		: allocator_(allocator)
		, code_ptr_(nullptr)
		, code_size_(0)
		, alloc_size_(0)
	{
	}

protected:
	machine_code_writer(machine_code_allocator &allocator, void *code_ptr, size_t code_size, size_t alloc_size)
		: allocator_(allocator)
		, code_ptr_(code_ptr)
		, code_size_(code_size)
		, alloc_size_(alloc_size)
	{
	}

public:
	/*template <typename T> void emit(T v)
	{
		ensure_capacity(sizeof(T));

		*((T *)(&((char*)(code_ptr_))[code_size_])) = v;
		code_size_ += sizeof(T);
	}

	void emit8(unsigned char c) { emit<decltype(c)>(c); }*/
	void emit16(unsigned short c) { copy_in(reinterpret_cast<const unsigned char *>(&c), sizeof(c)); }
	void emit32(unsigned int c) { copy_in(reinterpret_cast<const unsigned char *>(&c), sizeof(c)); }
	/*void emit64(unsigned long c) { emit<decltype(c)>(c); }*/

	void copy_in(const unsigned char *buffer, size_t size) {
		ensure_capacity(size);
		std::memcpy((unsigned char *)code_ptr_ + code_size_, buffer, size);

		code_size_ += size;
	}

	void shift(size_t offset, size_t size)
	{
		ensure_capacity(size);
		std::memmove((unsigned char *)code_ptr_ + offset, (unsigned char *)code_ptr_ + offset + size, code_size_ - offset);

		code_size_ += size;
	}

	void *ptr() const { return code_ptr_; }
	size_t size() const { return code_size_; }

	void finalise() {
		code_ptr_ = allocator_.allocate(code_ptr_, code_size_);
		__builtin___clear_cache(static_cast<char *>(code_ptr_), ((char *)code_ptr_ + code_size_));
		alloc_size_ = code_size_;

        util::global_logger.info("Machine code writer finalized: ptr={}, size={}\n", code_ptr_, alloc_size_);
	}

	void reset() {
		code_ptr_ = nullptr;
		alloc_size_ = code_size_ = 0;
	}

private:
	void ensure_capacity(size_t extra) {
		if ((code_size_ + extra) > alloc_size_) {
			if (alloc_size_ == 0) {
				alloc_size_ = 64;
			} else {
				alloc_size_ *= 2;
			}

			code_ptr_ = allocator_.allocate(code_ptr_, alloc_size_);
		}
	}

	machine_code_allocator &allocator_;
	void *code_ptr_;
	size_t code_size_;
	size_t alloc_size_;
};

} // namespace arancini::output::dynamic

