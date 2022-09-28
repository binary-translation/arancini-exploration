#include <arancini/runtime/exec/execution-context.h>
#include <arancini/runtime/exec/execution-thread.h>
#include <arancini/runtime/exec/x86/x86-cpu-state.h>
#include <iostream>

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

/*
 * Initialises the dynamic runtime for the guest program that is about to be executed.
 */
extern "C" void *initialise_dynamic_runtime(unsigned long entry_point)
{
	std::cerr << "arancini: dbt: initialise" << std::endl;

	ctx = new execution_context(ia, oe, 0x100000000);
	auto main_thread = ctx->create_execution_thread();

	// Initialise the CPU state structure with the PC set to the entry point of
	// the guest program, and a stack pointer at the top of the emulated address space.
	x86_cpu_state *x86_state = (x86_cpu_state *)main_thread->get_cpu_state();
	x86_state->pc = entry_point;
	x86_state->rsp = 0x100000000 - 8;

	std::cerr << "state @ " << (void *)x86_state << ", mem @ " << ctx->get_memory() << ", stack @ " << std::hex << x86_state->rsp << std::endl;

	return main_thread->get_cpu_state();
}

/*
 * Entry point from /static/ code when the CPU jumps to an address that hasn't been
 * translated.
 */
extern "C" int invoke_code(void *cpu_state) { return ctx->invoke(cpu_state); }
