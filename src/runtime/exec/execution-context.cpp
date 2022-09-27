#include <arancini/runtime/dbt/translation.h>
#include <arancini/runtime/exec/execution-context.h>
#include <arancini/runtime/exec/execution-thread.h>
#include <arancini/runtime/exec/x86/x86-cpu-state.h>
#include <asm/prctl.h>
#include <iostream>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

using namespace arancini::runtime::exec;

execution_context::execution_context(size_t memory_size)
	: memory_(nullptr)
	, memory_size_(memory_size)
{
	allocate_guest_memory();
}

execution_context::~execution_context() { }

void execution_context::allocate_guest_memory()
{
	memory_ = mmap(nullptr, memory_size_, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (memory_ == MAP_FAILED) {
		throw std::runtime_error("Unable to allocate guest memory");
	}

	// The GS register is used as the base address for the emulated guest memory.  Static and dynamic
	// code generate memory instructions based on this.
	syscall(SYS_arch_prctl, ARCH_SET_GS, (unsigned long long)memory_);
}

std::shared_ptr<execution_thread> execution_context::create_execution_thread()
{
	auto et = std::make_shared<execution_thread>(*this, sizeof(x86::x86_cpu_state));
	threads_[et->get_cpu_state()] = et;

	return et;
}

int execution_context::invoke(void *cpu_state)
{
	auto et = threads_[cpu_state];
	if (!et) {
		throw std::runtime_error("unable to resolve execution thread");
	}

	auto x86_state = (x86::x86_cpu_state *)cpu_state;

	std::cerr << "invoke PC=" << std::hex << x86_state->pc << std::endl;

	auto txln = te_.get_translation(x86_state->pc);
	if (txln == nullptr) {
		std::cerr << "unable to translate" << std::endl;
		return 1;
	}

	return txln->invoke(cpu_state);
}
