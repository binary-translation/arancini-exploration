#pragma once

#include <arancini/runtime/dbt/translation-engine.h>
#include <cstdlib>
#include <map>
#include <memory>

namespace arancini::runtime::exec {
class execution_thread;

class execution_context {
public:
	execution_context(size_t memory_size);
	~execution_context();

	std::shared_ptr<execution_thread> create_execution_thread();

	void *get_memory() const { return memory_; }

	int invoke(void *cpu_state);

private:
	void *memory_;
	size_t memory_size_;

	std::map<void *, std::shared_ptr<execution_thread>> threads_;
	dbt::translation_engine te_;

	void allocate_guest_memory();
};
} // namespace arancini::runtime::exec
