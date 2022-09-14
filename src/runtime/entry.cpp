#include <asm/prctl.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

struct xmmreg {
	unsigned long l, h;
};

// TODO: This shouldn't be hard coded.
struct cpu_state {
	/* 0 */ unsigned long pc;
	/* 1 */ unsigned long rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi;
	/* 9 */ unsigned long r8, r9, r10, r11, r12, r13, r14, r15;
	/* 17 */ unsigned char zf, cf, of, sf, pf;
	/* 22 */ xmmreg xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15;
	/* 38 */ unsigned long fs, gs;
};

void *mem;

/*
 * Initialises the dynamic runtime for the guest program that is about to be executed.
 */
extern "C" cpu_state *initialise_dynamic_runtime(unsigned long entry_point)
{
	std::cerr << "arancini: dbt: initialise" << std::endl;

	// Allocate storage for the emulated CPU state structure (TODO think about multithreading)
	auto s = new cpu_state();
	bzero(s, sizeof(*s));

	// Allocate emulated guest memory - hardcode this to 4Gb for now
	mem = mmap(nullptr, 0x100000000, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (mem == MAP_FAILED) {
		throw std::runtime_error("unable to map guest memory");
	}

	// The GS register is used as the base address for the emulated guest memory.  Static and dynamic
	// code generate memory instructions based on this.
	syscall(SYS_arch_prctl, ARCH_SET_GS, (unsigned long long)mem);

	// Initialise the CPU state structure with the PC set to the entry point of
	// the guest program, and a stack pointer at the top of the emulated address space.
	s->pc = entry_point;
	s->rsp = 0x100000000 - 8;

	std::cerr << "state @ " << (void *)s << ", mem @ " << mem << ", stack @ " << std::hex << s->rsp << std::endl;

	return s;
}

/*
 * Entry point from /static/ code when the CPU jumps to an address that hasn't been
 * translated.
 */
extern "C" int invoke_code(cpu_state *cpu_state)
{
	std::cerr << "arancini: dbt: invoke " << std::hex << cpu_state << std::endl;
	std::cerr << "PC=" << std::hex << cpu_state->pc << std::endl;

	return 1;
}
