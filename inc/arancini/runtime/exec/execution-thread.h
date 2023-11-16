#pragma once

#include <memory>

namespace arancini::runtime::exec {
class execution_context;

class execution_thread {
public:
	execution_thread(execution_context &owner, size_t state_size);
	~execution_thread();

	void *get_cpu_state() const { return cpu_state_; }

	uint64_t chain_address_;
	int* clear_child_tid_;
private:
	execution_context &owner_;
	void *cpu_state_;
	size_t cpu_state_size_;
};
} // namespace arancini::runtime::exec
