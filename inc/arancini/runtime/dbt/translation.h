#pragma once

#include <cstdlib>

namespace arancini::runtime::dbt {
extern "C" int call_native(void *, void *);

class translation {

public:
	translation(void *code_ptr, size_t code_size)
		: code_ptr_(code_ptr)
		, code_size_(code_size)
	{
	}

	~translation() { std::free(code_ptr_); }

	int invoke(void *cpu_state) { return call_native(code_ptr_, cpu_state); }

private:
	void *code_ptr_;
	size_t code_size_;
};
} // namespace arancini::runtime::dbt
