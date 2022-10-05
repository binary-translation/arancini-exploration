#include <arancini/runtime/exec/execution-context.h>
#include <arancini/runtime/exec/execution-thread.h>
#include <arancini/runtime/exec/x86/x86-cpu-state.h>
#include <iostream>

#include <signal.h>
#include <unistd.h>

#if defined(ARCH_X86_64)
#include <arancini/output/x86/x86-output-engine.h>
#elif defined(ARCH_ARM64)
#include <arancini/output/arm64/arm64-output-engine.h>
#endif

#include <arancini/input/x86/x86-input-arch.h>

using namespace arancini::runtime::exec;
using namespace arancini::runtime::exec::x86;

static execution_context *ctx;

// TODO: this needs to depend on something, somehow.  Some kind of variable?
static arancini::input::x86::x86_input_arch ia;

#if defined(ARCH_X86_64)
static arancini::output::x86::x86_output_engine oe;
#elif defined(ARCH_ARM64)
static arancini::output::arm64::arm64_output_engine oe;
#else
#error "Unsupported output architecture"
#endif

static void segv_handler(int signo, siginfo_t *info, void *context)
{
	std::cerr << "SEGMENTATION FAULT: code=" << std::hex << info->si_code << ", host-virtual-address=" << std::hex << info->si_addr;

	uintptr_t emulated_base = (uintptr_t)ctx->get_memory_ptr(0);
	if ((uintptr_t)info->si_addr >= emulated_base) {
		std::cerr << ", guest-virtual-address=" << std::hex << (info->si_addr - emulated_base);
	}

	std::cerr << std::endl;

	exit(1);
}

/*
 * Initialises signal handling
 */
static void init_signals()
{
	struct sigaction sa = { 0 };

	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = &segv_handler;
	if (sigaction(SIGSEGV, &sa, nullptr) < 0) {
		throw std::runtime_error("unable to initialise signal handling");
	}
}

/*
 * Initialises the dynamic runtime for the guest program that is about to be executed.
 */
extern "C" void *initialise_dynamic_runtime(unsigned long entry_point)
{
	std::cerr << "arancini: dbt: initialise" << std::endl;

	init_signals();

	// Create an execution context for the given input (guest) and output (host) architecture.
	ctx = new execution_context(ia, oe);

	// Create a memory area for the stack.
	unsigned long stack_size = 0x10000;
	ctx->add_memory_region(0x100000000 - stack_size, stack_size);

	// TODO: Load guest .text, .data, .bss sections via program headers

	auto main_thread = ctx->create_execution_thread();

	// Initialise the CPU state structure with the PC set to the entry point of
	// the guest program, and an emulated stack pointer at the top of the
	// emulated address space.
	x86_cpu_state *x86_state = (x86_cpu_state *)main_thread->get_cpu_state();
	x86_state->pc = entry_point;
	x86_state->rsp = 0x100000000 - 8;

	std::cerr << "state @ " << (void *)x86_state << ", pc @ " << std::hex << x86_state->pc << ", stack @ " << std::hex << x86_state->rsp << std::endl;

	return main_thread->get_cpu_state();
}

/*
 * Entry point from /static/ code when the CPU jumps to an address that hasn't been
 * translated.
 */
extern "C" int invoke_code(void *cpu_state) { return ctx->invoke(cpu_state); }
