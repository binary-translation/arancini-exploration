#pragma once

#include <cstdint>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <sys/mman.h>

namespace arancini::output::dynamic {
class machine_code_allocator {
public:
	virtual void *allocate(void *original, size_t size) = 0;
};

class arena {
public:
	arena(size_t size)
		: base_(nullptr)
		, size_(size)
	{
		// Attempt to allocate the arena memory area.
		allocate();
	}

	~arena()
	{
		// Free the arena memory area.
		free();
	}

	void *base() const { return base_; }
	size_t size() const { return size_; }

private:
	void *base_;
	size_t size_;

	void allocate()
	{
		// Use MMAP with the appropriate permissions to allow execution.  TODO: think about locking/unlocking the region
		// to support W^X
		base_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

		if (base_ == MAP_FAILED) {
			throw std::runtime_error("failed to allocate memory for arena");
		}
	}

	void free() { munmap(base_, size_); }
};

class arena_machine_code_allocator : public machine_code_allocator {
public:
	arena_machine_code_allocator(arena &a)
		: arena_(a)
		, next_allocation_(a.base())
		, current_allocation_(nullptr)
		, current_allocation_size_(0)
	{
	}

	virtual void *allocate(void *original, size_t size) override
	{
		if (original == nullptr) {
			// Increment the next allocation by the size of the current allocation, aligned
			// by 16 bytes.
			next_allocation_ = (void *)((uintptr_t)next_allocation_ + ((current_allocation_size_ + 15) & ~0xfull));

			if ((uintptr_t)next_allocation_ > ((uintptr_t)arena_.base() + arena_.size())) {
				throw std::runtime_error("out of memory");
			}

			// Record the size and pointer of the current allocation.
			current_allocation_size_ = size;
			current_allocation_ = next_allocation_;

			// Return the current allocation
			return current_allocation_;
		} else {
			// Adjustments can only be made to the current allocation.
			if (original != current_allocation_) {
				throw std::runtime_error("multiple allocations not supported");
			}

			// Modify the current allocation size, and return the original allocation.
			current_allocation_size_ = size;
			return original;
		}
	}

private:
	arena &arena_;
	void *next_allocation_, *current_allocation_;
	size_t arena_size_, current_allocation_size_;
};
} // namespace arancini::output::dynamic
