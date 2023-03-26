#include <arancini/runtime/dbt/translation.h>
#include <arancini/runtime/exec/execution-context.h>
#include <arancini/runtime/exec/execution-thread.h>
#include <arancini/runtime/exec/native_syscall.h>
#include <arancini/runtime/exec/x86/x86-cpu-state.h>

#if defined(ARCH_X86_64)
#include <asm/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#define GM_BASE (void *)0x600000000000ull
#else
#define GM_BASE nullptr
#endif

#include <asm-generic/ioctls.h>
#include <sys/uio.h>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <sys/mman.h>

using namespace arancini::runtime::exec;

execution_context::execution_context(input::input_arch &ia, output::dynamic::dynamic_output_engine &oe)
	: memory_(nullptr)
	, memory_size_(0x100000000ull)
	, te_(*this, ia, oe)
{
	allocate_guest_memory();
}

execution_context::~execution_context() { }

void *execution_context::add_memory_region(off_t base_address, size_t size)
{
	if ((base_address + size) > memory_size_) {
		throw std::runtime_error("memory region out of bounds");
	}

	uintptr_t base_ptr = (uintptr_t)memory_ + base_address;
	uintptr_t aligned_base_ptr = base_ptr & ~0xfffull;
	uintptr_t base_ptr_off = base_ptr & 0xfffull;
	uintptr_t aligned_size = (size + base_ptr_off + 0xfff) & ~0xfffull;

	std::cerr << "amr: base-pointer=" << std::hex << base_ptr << ", aligned-base-ptr=" << aligned_base_ptr << ", base-ptr-off=" << base_ptr_off
			  << ", size=" << size << ", aligned-size=" << aligned_size << std::endl;

	mprotect((void *)aligned_base_ptr, aligned_size, PROT_READ | PROT_WRITE);

	return (void *)base_ptr;
}

void execution_context::allocate_guest_memory()
{
	memory_ = mmap(GM_BASE, memory_size_, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (memory_ == MAP_FAILED) {
		throw std::runtime_error("Unable to allocate guest memory");
	}

#if defined(ARCH_X86_64)
	// The GS register is used as the base address for the emulated guest memory.  Static and dynamic
	// code generate memory instructions based on this.
	syscall(SYS_arch_prctl, ARCH_SET_GS, (unsigned long long)memory_);
#endif
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

	std::cerr << "=================" << std::endl;
	std::cerr << "INVOKE PC=" << std::hex << x86_state->PC << std::endl;
	std::cerr << "=================" << std::endl;

	auto txln = te_.get_translation(x86_state->PC);
	if (txln == nullptr) {
		std::cerr << "unable to translate" << std::endl;
		return 1;
	}

	return txln->invoke(cpu_state, memory_);
}

int execution_context::internal_call(void *cpu_state, int call)
{
	std::cerr << "Executing internal call via TEMPORARY interface" << std::endl;
	if (call == 1) { // syscall
		auto x86_state = (x86::x86_cpu_state *)cpu_state;
		switch (x86_state->RAX) {
		case 16: // ioctl
		{
			// Not sure how many actually needed

			uint64_t arg = x86_state->RDX;

			uint64_t request = x86_state->RSI;

			switch (request) {
			case TIOCGWINSZ:
				arg += (uintptr_t)memory_;
				break;
			default:
				std::cerr << "Unknown ioctl request " << std::dec << request << std::endl;
				x86_state->RAX = -EINVAL;
				return 0;
			}

			x86_state->RAX = native_syscall(__NR_ioctl, x86_state->RDI, request, arg);
			break;
		}
		case 20: // writev
		{

			auto iovec = (const struct iovec *)(x86_state->RSI + (uintptr_t(memory_)));

			auto iocnt = x86_state->RDX;

			struct iovec iovec_new[iocnt];

			for (auto i = 0ull; i < iocnt; ++i) {
				iovec_new[i].iov_base = reinterpret_cast<void *>((uintptr_t)iovec[i].iov_base + (uintptr_t)memory_);
				iovec_new[i].iov_len = iovec[i].iov_len;
			}

			x86_state->RAX = native_syscall(__NR_writev, x86_state->RDI, (uintptr_t)iovec_new, iocnt);
			break;
		}
		case 158: // arch_prctl
		{
			switch (x86_state->RDI) { // code
			case 0x1001: // ARCH_SET_GS
				x86_state->GS = x86_state->RSI;
				x86_state->RAX = 0;
				break;
			case 0x1002: // ARCH_SET_FS
				x86_state->FS = x86_state->RSI;
				x86_state->RAX = 0;
				break;
			case 0x1003: // ARCH_GET_FS
				(*((uint64_t *)(x86_state->RSI + (intptr_t)memory_))) = x86_state->FS;
				x86_state->RAX = 0;
				break;
			case 0x1004: // ARCH_GET_GS
				(*((uint64_t *)(x86_state->RSI + (intptr_t)memory_))) = x86_state->GS;
				x86_state->RAX = 0;
				break;
			default:
				x86_state->RAX = -EINVAL;
			}
			break;
		}
		case 218: // set_tid_address
		{
			// TODO Handle clear_child_tid in exit
			auto et = threads_[cpu_state];
			et->clear_child_tid_ = (int *)(x86_state->RDI + (uintptr_t)memory_);
			x86_state->RAX = gettid();
			break;
		}
		case 231://exit_group
		{
			std::cerr << "Exiting from emulated process with exit code " << std::dec << x86_state->RDI << std::endl;
			//exit(x86_state->RDI) ?
			return 1;
		}
		default:
			std::cerr << "Unsupported syscall id " << std::dec << x86_state->RAX << std::endl;
			return 1;
		}
	} else {
		std::cerr << "Unknown internal call id " << std::dec << call << std::endl;
		return 1;
	}
	return 0;
}