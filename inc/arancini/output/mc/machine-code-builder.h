#pragma once

#include <cstdlib>
#include <memory>

#include <arancini/output/mc/machine-code-allocator.h>

namespace arancini::output::mc {
class machine_code_builder {
public:
	machine_code_builder(std::shared_ptr<machine_code_allocator> allocator)
		: allocator_(allocator)
		, base_((unsigned char *)allocator->alloc(16))
		, offset_(0)
	{
	}

	const void *get_base() const { return base_; }
	size_t get_size() const { return offset_; }

	void write_u8(unsigned char v)
	{
		ensure_capacity(1);
		base_[offset_++] = v;
	}
	void write_u16(unsigned short v);
	void write_u32(unsigned int v);
	void write_u64(unsigned long int v);

private:
	std::shared_ptr<machine_code_allocator> allocator_;
	unsigned char *base_;
	off_t offset_;

	void ensure_capacity(off_t additional_storage)
	{
		// if (offset)
	}
};
} // namespace arancini::output::mc
