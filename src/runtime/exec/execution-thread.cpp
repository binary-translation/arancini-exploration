#include <arancini/runtime/exec/execution-thread.h>
#include <cstdlib>
#include <stdexcept>

using namespace arancini::runtime::exec;

execution_thread::execution_thread(execution_context &owner, size_t state_size)
	: chain_address_(0)
	, owner_(owner)
	, cpu_state_(nullptr)
	, cpu_state_size_(state_size)
	, run_time(0.0)
	, guest_time(0.0)
{
	cpu_state_ = std::malloc(state_size);
	if (!cpu_state_) {
		throw std::runtime_error("unable to allocate storage for CPU state");
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &entry_);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &exit_);
}

execution_thread::~execution_thread() {
	std::free(cpu_state_);
	std::cerr << "Time spent in runtime+dynamic: " << std::dec << run_time << "s." << std::endl;
	std::cerr << "Time spent in guest static translation: " << std::dec << guest_time << "s." << std::endl;
}
