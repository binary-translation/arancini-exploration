#include <arancini/runtime/exec/execution-thread.h>
#include <cstdlib>
#include <stdexcept>

using namespace arancini::runtime::exec;

execution_thread::execution_thread(execution_context &owner, size_t state_size)
	: owner_(owner)
	, cpu_state_(nullptr)
	, cpu_state_size_(state_size)
{
	cpu_state_ = std::malloc(state_size);
	if (!cpu_state_) {
		throw std::runtime_error("unable to allocate storage for CPU state");
	}
}

execution_thread::~execution_thread() { std::free(cpu_state_); }
