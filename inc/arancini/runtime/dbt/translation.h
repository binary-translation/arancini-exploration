#pragma once

#include <cstdlib>
#include <iostream>

namespace arancini::runtime::dbt {

struct native_call_result {
	int exit_code;
	uint64_t chain_address;
};

extern "C" native_call_result call_native(void *, void *, void *);

class translation {

public:
	translation(void *code_ptr, size_t code_size)
		: code_ptr_(code_ptr)
		, code_size_(code_size)
	{
	}

	~translation() { std::free(code_ptr_); }

	native_call_result invoke(void *cpu_state, void *mem_base) { return call_native(code_ptr_, cpu_state, mem_base); }

private:
	void *code_ptr_;
	size_t code_size_;
};
} // namespace arancini::runtime::dbt
