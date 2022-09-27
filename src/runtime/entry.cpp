#include <arancini/runtime/exec/execution-context.h>
#include <arancini/runtime/exec/execution-thread.h>
#include <arancini/runtime/exec/x86/x86-cpu-state.h>
#include <iostream>

using namespace arancini::runtime::exec;
using namespace arancini::runtime::exec::x86;

static execution_context *ctx;

/*
 * Initialises the dynamic runtime for the guest program that is about to be executed.
 */
extern "C" void *initialise_dynamic_runtime(unsigned long entry_point)
{
	std::cerr << "arancini: dbt: initialise" << std::endl;

	ctx = new execution_context(0x100000000);
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
