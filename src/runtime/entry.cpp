#include <arancini/runtime/exec/execution-context.h>
#include <arancini/runtime/exec/execution-thread.h>
#include <arancini/runtime/exec/x86/x86-cpu-state.h>
#include <cstring>
#include <iostream>

#include <mutex>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>

#if defined(ARCH_X86_64)
#include <arancini/output/dynamic/x86/x86-dynamic-output-engine.h>
#elif defined(ARCH_AARCH64)
#include <arancini/output/dynamic/arm64/arm64-dynamic-output-engine.h>
#elif defined(ARCH_RISCV64)
#include <arancini/output/dynamic/riscv64/riscv64-dynamic-output-engine.h>
#else
#error "Unsupported dynamic output architecture"
#endif

#include <arancini/input/x86/x86-input-arch.h>
#include <sys/auxv.h>
#include <sys/ucontext.h>

using namespace arancini::runtime::exec;
using namespace arancini::runtime::exec::x86;

static execution_context *ctx_;

// TODO: this needs to depend on something, somehow.  Some kind of variable?
static arancini::input::x86::x86_input_arch ia(true, arancini::input::x86::disassembly_syntax::intel);

#if defined(ARCH_X86_64)
static arancini::output::dynamic::x86::x86_dynamic_output_engine oe;
#elif defined(ARCH_AARCH64)
static arancini::output::dynamic::arm64::arm64_dynamic_output_engine oe;
#elif defined(ARCH_RISCV64)
static arancini::output::dynamic::riscv64::riscv64_dynamic_output_engine oe;
#else
#error "Unsupported dynamic output architecture"
#endif

// HACK: for Debugging
static x86_cpu_state *__current_state;

static std::mutex segv_lock;
/*
 * The segfault handler.
 */
static void segv_handler(int signo, siginfo_t *info, void *context)
{
	segv_lock.lock();
#if defined(ARCH_X86_64)
	unsigned long rip = ((ucontext_t *)context)->uc_mcontext.gregs[REG_RIP];
#else
	unsigned long rip = 0;
#endif

	std::cerr << "SEGMENTATION FAULT: code=" << std::hex << info->si_code << ", rip=" << std::hex << rip << ", host-virtual-address=" << std::hex
			  << info->si_addr;

	uintptr_t emulated_base = (uintptr_t)ctx_->get_memory_ptr(0);
	if ((uintptr_t)info->si_addr >= emulated_base) {
		std::cerr << ", guest-virtual-address=" << std::hex << ((uintptr_t)info->si_addr - emulated_base) << std::endl;
	}

	unsigned i = 0;
	auto range = ctx_->get_thread_range();
	for (auto it  = range.first; it != range.second; it++) {
			std::cerr << "Thread[" << i << "] Guest PC: " << ((x86_cpu_state *)it->second->get_cpu_state())->PC << std::endl;
			i++;
	}

	segv_lock.unlock();
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
	std::cerr << "loading gph load-addr=" << std::hex << md->load_address << ", mem-size=" << md->memory_size
			  << ", end=" << (md->load_address + md->memory_size) << ", file-size=" << md->file_size << ", target=" << ptr << std::endl;

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

static uint64_t setup_guest_stack(int argc, char **argv, intptr_t stack_top, execution_context *execution_context, int start)
{
	// Stack pointer always needs to be 16-Byte aligned per ABI convention
	int envc = 0;
	for (; environ[envc]; ++envc)
		;

	// auxv entries are always 16 Bytes
	stack_top -= ((envc + (argc - (start - 1))) & 1) * 8;

	// Add auxv to guest stack
	{
		auto *stack = (Elf64_auxv_t *)execution_context->get_memory_ptr(stack_top);
		*(--stack) = (Elf64_auxv_t) { AT_NULL, { 0 } };
		//		*(--stack) = (Elf64_auxv_t) {AT_ENTRY, {...}};
		//		*(--stack) = (Elf64_auxv_t) {AT_PHDR, {...r}};
		//		*(--stack) = (Elf64_auxv_t) {AT_PHNUM, {...}};
		//		*(--stack) = (Elf64_auxv_t) {AT_PHENT, {...}};
		*(--stack) = (Elf64_auxv_t) { AT_UID, { getauxval(AT_UID) } };
		*(--stack) = (Elf64_auxv_t) { AT_GID, { getauxval(AT_GID) } };
		*(--stack) = (Elf64_auxv_t) { AT_EGID, { getauxval(AT_EGID) } };
		*(--stack) = (Elf64_auxv_t) { AT_EUID, { getauxval(AT_EUID) } };
		*(--stack) = (Elf64_auxv_t) { AT_CLKTCK, { getauxval(AT_CLKTCK) } };
		*(--stack) = (Elf64_auxv_t) { AT_RANDOM, { getauxval(AT_RANDOM) - (uintptr_t)execution_context->get_memory_ptr(0) } }; // TODO Copy/Generate new one?
		*(--stack) = (Elf64_auxv_t) { AT_SECURE, { 0 } };
		*(--stack) = (Elf64_auxv_t) { AT_PAGESZ, { getauxval(AT_PAGESZ) } };
		*(--stack) = (Elf64_auxv_t) { AT_HWCAP, { 0 } };
		*(--stack) = (Elf64_auxv_t) { AT_HWCAP2, { 0 } };
		//        *(--stack) = (Elf64_auxv_t) {AT_PLATFORM, {0}};
		*(--stack) = (Elf64_auxv_t) { AT_EXECFN, { (uintptr_t)argv[0] - (uintptr_t)execution_context->get_memory_ptr(0) } };
		stack_top = (intptr_t)(stack) - (intptr_t)execution_context->get_memory_ptr(0);
	}
	// Copy environ to guest stack
	{
		char **stack = (char **)execution_context->get_memory_ptr(stack_top);
		*(--stack) = nullptr;
		// Zero terminated so environ[envc] will be zero and also needs to be copied
		for (int i = envc - 1; i >= 0; i--) {
			*(--stack) = (char *)(((uintptr_t)environ[i]) - (uintptr_t)execution_context->get_memory_ptr(0));
		}
		stack_top = (intptr_t)stack - (intptr_t)execution_context->get_memory_ptr(0);
	}
	// Copy argv to guest stack
	{
		const char **stack = (const char **)execution_context->get_memory_ptr(stack_top);

		// Zero terminated so argv[argc] will be zero and also needs to be copied
		*(--stack) = nullptr;
		for (int i = argc - 1; i >= start; i--) {
			*(--stack) = (char *)(((uintptr_t)argv[i]) - (uintptr_t)execution_context->get_memory_ptr(0));
		}
		*(--stack) = (char *)(((uintptr_t)argv[0]) - (uintptr_t)execution_context->get_memory_ptr(0));
		stack_top = (intptr_t)stack - (intptr_t)execution_context->get_memory_ptr(0);
	}
	// Copy argc to guest stack
	{
		long *stack = (long *)execution_context->get_memory_ptr(stack_top);
		*(--stack) = argc - (start - 1);
		stack_top = (intptr_t)stack - (intptr_t)execution_context->get_memory_ptr(0);
	}

	return (intptr_t)stack_top;
}

/*
 * Initialises the dynamic runtime for the guest program that is about to be executed.
 */
extern "C" void *initialise_dynamic_runtime(unsigned long entry_point, int argc, char **argv)
{
	std::cerr << "arancini: dbt: initialise" << std::endl;

	// Consume args until '--'
	int start = 1;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--") == 0) {
			start = i + 1;
		}
	}

	bool optimise = start > 1;

	// Capture interesting signals, such as SIGSEGV.
	init_signals();

	// Create an execution context for the given input (guest) and output (host) architecture.
	ctx_ = new execution_context(ia, oe, optimise);

	// Create a memory area for the stack.
	unsigned long stack_size = 0x10000;
	ctx_->add_memory_region(0x100000000 - stack_size, stack_size, true);

	// TODO: Load guest .text, .data, .bss sections via program headers
	load_guest_program_headers(ctx_);

	// Create the main execution thread.
	auto main_thread = ctx_->create_execution_thread();

	// Initialise the CPU state structure with the PC set to the entry point of
	// the guest program, and an emulated stack pointer at the top of the
	// emulated address space.
	x86_cpu_state *x86_state = (x86_cpu_state *)main_thread->get_cpu_state();
	__current_state = x86_state;
	x86_state->PC = entry_point;

	x86_state->GS = (unsigned long long)ctx_->get_memory_ptr(0);
	x86_state->RSP = setup_guest_stack(argc, argv, 0x100000000, ctx_, start);
	x86_state->X87_STACK_BASE = (intptr_t)mmap(NULL, 80, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0) - (intptr_t)ctx_->get_memory_ptr(0);
	// Report on various information for useful debugging purposes.
	std::cerr << "state @ " << (void *)x86_state << ", pc @ " << std::hex << x86_state->PC << ", stack @ " << std::hex << x86_state->RSP << std::endl;

	// Initialisation of the runtime is complete - return a pointer to the raw CPU state structure
	// so that the static code can use it for emulation.
	return main_thread->get_cpu_state();
}

/*
 * Entry point from /static/ code when the CPU jumps to an address that hasn't been
 * translated.
 */
extern "C" int invoke_code(void *cpu_state) { return ctx_->invoke(cpu_state); }

/*
 * Entry point from /static/ code when internal call needs to be executed.
 */
extern "C" int execute_internal_call(void *cpu_state, int call) { return ctx_->internal_call(cpu_state, call); }

extern "C" void finalize() { delete ctx_; exit(0); }

extern "C" void alert() { std::cout << "Top of MainLoop!\n"; }
