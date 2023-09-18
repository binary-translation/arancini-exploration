#pragma once

#include <arancini/output/dynamic/machine-code-allocator.h>
#include <arancini/output/dynamic/machine-code-writer.h>

namespace arancini::output::dynamic {
/**
 * Allocates at a given address to allow overwriting code at that address in chaining.
 * Performs no checks of bounds or anything.
 */
class chain_machine_code_allocator : public machine_code_allocator {
public:
	explicit chain_machine_code_allocator(void *target)
		: target_(target)
	{
	}
	void *allocate(void *original, size_t) override
	{
		if (original == nullptr) {
			return target_;
		}
		return original;
	}

private:
	void *target_;
};

/**
 * Writes code to a given address to allow overwriting code at that address in chaining.
 */
class chain_machine_code_writer : public machine_code_writer {
public:
	explicit chain_machine_code_writer(void *target)
		: machine_code_writer(alloc_)
		, alloc_(target)
	{
		finalise(); // FIXME Hack to populate ptr
	}

	~chain_machine_code_writer() { finalise(); }

private:
	chain_machine_code_allocator alloc_;
};
} // namespace arancini::output::dynamic
