#include <arancini/runtime/exec/execution-context.h>
#include <arancini/runtime/exec/execution-thread.h>
#include <arancini/runtime/exec/x86/x86-cpu-state.h>
#include <cstring>
#include <iostream>

#include <signal.h>
#include <unistd.h>

#if defined(ARCH_X86_64)
#include <arancini/output/dynamic/x86/x86-output-engine.h>
#elif defined(ARCH_AARCH64)
#include <arancini/output/dynamic/arm64/arm64-output-engine.h>
#elif defined(ARCH_RISCV)
#include <arancini/output/dynamic/riscv/riscv-output-engine.h>
#else
#error "Unsupported dynamic output architecture"
#endif

#include <arancini/input/x86/x86-input-arch.h>

using namespace arancini::runtime::exec;
using namespace arancini::runtime::exec::x86;

static execution_context *ctx;

// TODO: this needs to depend on something, somehow.  Some kind of variable?
static arancini::input::x86::x86_input_arch ia(arancini::input::x86::disassembly_syntax::intel);

#if defined(ARCH_X86_64)
static arancini::output::dynamic::x86::x86_output_engine oe;
#elif defined(ARCH_AARCH64)
static arancini::output::dynamic::arm64::arm64_output_engine oe;
#elif defined(ARCH_RISCV)
static arancini::output::dynamic::riscv::riscv_output_engine oe;
#else
#error "Unsupported dynamic output architecture"
#endif

/*
 * The segfault handler.
 */
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

	// Capture SIGSEGV
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = &segv_handler;
	if (sigaction(SIGSEGV, &sa, nullptr) < 0) {
		throw std::runtime_error("unable to initialise signal handling");
	}
}

// Represents the metadata that stored in the host binary about the guest program headers.
struct guest_program_header_metadata {
	unsigned long load_address;
	unsigned long file_size;
	unsigned long memory_size;
	char data[];
} __attribute__((packed));

// The symbol that contains the list of pointers to the guest program header metadata structures.
extern "C" const guest_program_header_metadata *__GPH[];

/*
 * Loads a guest program header into emulated memory.
 */
static void load_gph(execution_context *ctx, const guest_program_header_metadata *md)
{
	if (md->file_size > md->memory_size) {
		throw std::runtime_error("cannot have gph file size larger than memory size");
	}

	// Create the guest memory region where the program header will be loaded into.
	// This should be at the specified load address, and of the specified memory size.
	void *ptr = ctx->add_memory_region(md->load_address, md->memory_size);

	// Debugging information
	std::cerr << "loading gph load-addr=" << std::hex << md->load_address << ", mem-size=" << md->memory_size << ", file-size=" << md->file_size
			  << ", target=" << ptr << std::endl;

	// Copy the data from the host binary into the new allocated region of emulated
	// guest memory.  This should be only of the specified file size, because the file size
	// of the data can be smaller than the memory size (e.g. BSS).
	std::memcpy(ptr, md->data, md->file_size);
}

/*
 * Loads the guest program headers from the host binary into emulated guest memory.
 */
static void load_guest_program_headers(execution_context *ctx)
{
	// Get a pointer to the list of pointers to the metadata structures.
	const guest_program_header_metadata **gphp = __GPH;

	// Loop over the pointers until null.
	while (*gphp) {
		// Trigger loading of the GPH.
		load_gph(ctx, *gphp);

		// Advance the pointer.
		gphp++;
	}
}

/*
 * Initialises the dynamic runtime for the guest program that is about to be executed.
 */
extern "C" void *initialise_dynamic_runtime(unsigned long entry_point)
{
	std::cerr << "arancini: dbt: initialise" << std::endl;

	// Capture interesting signals, such as SIGSEGV.
	init_signals();

	// Create an execution context for the given input (guest) and output (host) architecture.
	ctx = new execution_context(ia, oe);

	// Create a memory area for the stack.
	unsigned long stack_size = 0x10000;
	ctx->add_memory_region(0x100000000 - stack_size, stack_size);

	// TODO: Load guest .text, .data, .bss sections via program headers
	load_guest_program_headers(ctx);

	// Create the main execution thread.
	auto main_thread = ctx->create_execution_thread();

	// Initialise the CPU state structure with the PC set to the entry point of
	// the guest program, and an emulated stack pointer at the top of the
	// emulated address space.
	x86_cpu_state *x86_state = (x86_cpu_state *)main_thread->get_cpu_state();
	x86_state->pc = entry_point;
	x86_state->rsp = 0x100000000 - 8;

	// Report on various information for useful debugging purposes.
	std::cerr << "state @ " << (void *)x86_state << ", pc @ " << std::hex << x86_state->pc << ", stack @ " << std::hex << x86_state->rsp << std::endl;

	// Initialisation of the runtime is complete - return a pointer to the raw CPU state structure
	// so that the static code can use it for emulation.
	return main_thread->get_cpu_state();
}

/*
 * Entry point from /static/ code when the CPU jumps to an address that hasn't been
 * translated.
 */
extern "C" int invoke_code(void *cpu_state) { return ctx->invoke(cpu_state); }
