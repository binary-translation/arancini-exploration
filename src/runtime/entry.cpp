#include <asm/prctl.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

struct cpu_state {
	unsigned long pc;
	unsigned long rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi;
	unsigned long r8, r9, r10, r11, r12, r13, r14, r15;
	unsigned char zf, cf, of, sf;
};

void *mem;

extern "C" cpu_state *initialise_dynamic_runtime(unsigned long entry_point)
{
	std::cerr << "arancini: dbt: initialise" << std::endl;
	auto s = new cpu_state();
	bzero(s, sizeof(s));

	mem = mmap(nullptr, 0x100000000, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	if (mem == MAP_FAILED) {
		throw std::runtime_error("unable to map guest memory");
	}

	// asm volatile("int3");
	syscall(SYS_arch_prctl, ARCH_SET_GS, (unsigned long long)mem);

	//_writegsbase_u64((unsigned long long)mem);

	s->pc = entry_point;
	s->rsp = 0x100000000 - 8;

	std::cerr << "state @ " << (void *)s << ", mem @ " << mem << ", stack @ " << std::hex << s->rsp << std::endl;

	// asm volatile("int3");
	return s;
}

extern "C" int invoke_code(cpu_state *cpu_state)
{
	std::cerr << "arancini: dbt: invoke " << std::hex << cpu_state << std::endl;
	std::cerr << "PC=" << std::hex << cpu_state->pc << std::endl;

	return 1;
}
