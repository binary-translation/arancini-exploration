#pragma once

#include <arancini/runtime/dbt/translation-engine.h>
#include <cstdlib>
#include <map>
#include <memory>

namespace arancini::input {
class input_arch;
}

namespace arancini::output::dynamic {
class dynamic_output_engine;
}

namespace arancini::runtime::exec {
class execution_thread;

class execution_context {
public:
	execution_context(input::input_arch &ia, output::dynamic::dynamic_output_engine &oe, bool optimise);
	~execution_context();

	void *add_memory_region(off_t base_address, size_t size, bool ignore_brk=false);

	std::shared_ptr<execution_thread> create_execution_thread();

	void *get_memory_ptr(off_t base_address) const { return (void *)((uintptr_t)memory_ + base_address); }

	int invoke(void *cpu_state);
	int internal_call(void *cpu_state, int call);

private:
	void *memory_;
	size_t memory_size_;
	uintptr_t brk_;
	uintptr_t brk_limit_;

	std::map<void *, std::shared_ptr<execution_thread>> threads_;
	dbt::translation_engine te_;

	void allocate_guest_memory();
};
} // namespace arancini::runtime::exec
