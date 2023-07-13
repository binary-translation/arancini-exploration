#include <arancini/runtime/dbt/translation.h>
#include <arancini/runtime/exec/execution-context.h>
#include <arancini/runtime/exec/execution-thread.h>
#include <arancini/runtime/exec/native_syscall.h>
#include <arancini/runtime/exec/x86/x86-cpu-state.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <pthread.h>

#if defined(ARCH_X86_64)
#include <asm/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#define GM_BASE (void *)0x600000000000ull
#else
#define GM_BASE nullptr
#endif

#include <asm-generic/ioctls.h>
#include <asm/stat.h>
#include <csignal>
#include <iostream>
#include <linux/fcntl.h>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/uio.h>

extern "C" int MainLoop(void *);

using namespace arancini::runtime::exec;

struct loop_args {
	void *new_state;
	void *parent_state;
	pthread_mutex_t *lock;
	pthread_cond_t *cond;
};

void *MainLoopWrapper(void *args) {
	auto largs = (loop_args *)args;
	auto x86_state = (x86::x86_cpu_state *)largs->new_state;
	auto parent_state = (x86::x86_cpu_state *)largs->parent_state;

	pthread_mutex_lock(largs->lock);
	parent_state->RAX = gettid();
	pthread_mutex_unlock(largs->lock);
	pthread_cond_signal(largs->cond);

	x86_state->RSP = x86_state->RSI;
	
	MainLoop(x86_state);
	return NULL;
};

execution_context::execution_context(input::input_arch &ia, output::dynamic::dynamic_output_engine &oe, bool optimise)
	: memory_(nullptr)
	, memory_size_(0x100000000ull)
	, brk_ { 0 }
	, brk_limit_ { UINTPTR_MAX }
	, te_(*this, ia, oe, optimise)
{
	allocate_guest_memory();
}

execution_context::~execution_context() { }

void *execution_context::add_memory_region(off_t base_address, size_t size, bool ignore_brk)
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

	if (!ignore_brk) {
		brk_ = std::max(aligned_base_ptr + aligned_size, brk_);
	} else {
		brk_limit_ = std::min(brk_limit_, aligned_base_ptr);
	}

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
		std::cerr << "Syscall No " << std::dec << x86_state->RAX << std::endl;
		switch (x86_state->RAX) {
		case 2: // open
		{
			auto filename = (uintptr_t)get_memory_ptr((off64_t)x86_state->RDI);
			uint64_t flags = x86_state->RSI;
			uint64_t mode = x86_state->RDX;
			x86_state->RAX = native_syscall(__NR_openat, (uint64_t)AT_FDCWD, filename, flags, mode);
			break;
		}
		case 3: // close
		{
			x86_state->RAX = native_syscall(__NR_close, x86_state->RDI);
			break;
		}
		case 5: // fstat
		{
			uint64_t fd = x86_state->RDI;
			uint64_t statp = x86_state->RSI;
			struct stat tmp_struct { };

			uint64_t result = native_syscall(__NR_fstat, fd, (uintptr_t)&tmp_struct);
			x86_state->RAX = result;

			if (result == 0) {

				struct target_stat {
					unsigned long st_dev;
					unsigned long st_ino;
					unsigned long st_nlink;

					unsigned int st_mode;
					unsigned int st_uid;
					unsigned int st_gid;
					unsigned int __pad0;
					unsigned long st_rdev;
					long st_size;
					long st_blksize;
					long st_blocks;

					unsigned long st_atime;
					unsigned long st_atime_nsec;
					unsigned long st_mtime;
					unsigned long st_mtime_nsec;
					unsigned long st_ctime;
					unsigned long st_ctime_nsec;
					long __unused[3];
				} __attribute__((packed)) *target = (struct target_stat *)get_memory_ptr((off_t)statp);

				target->st_dev = tmp_struct.st_dev;
				target->st_ino = tmp_struct.st_ino;
				target->st_nlink = tmp_struct.st_nlink;
				target->st_mode = tmp_struct.st_mode;
				target->st_uid = tmp_struct.st_uid;
				target->st_gid = tmp_struct.st_gid;
				target->st_rdev = tmp_struct.st_rdev;
				target->st_size = tmp_struct.st_size;
				target->st_blksize = tmp_struct.st_blksize;
				target->st_blocks = tmp_struct.st_blocks;
				target->st_atime = tmp_struct.st_atime;
				target->st_atime_nsec = tmp_struct.st_atime_nsec;
				target->st_mtime = tmp_struct.st_mtime;
				target->st_mtime_nsec = tmp_struct.st_mtime_nsec;
				target->st_ctime = tmp_struct.st_ctime;
				target->st_ctime_nsec = tmp_struct.st_ctime_nsec;
			}
			break;
		}
		case 9: // mmap
		{
			// Hint to higher than already mapped memory if no hint
			auto addr = x86_state->RDI == 0 ? (uintptr_t)get_memory_ptr((off_t)memory_size_ + 4096) : (uintptr_t)get_memory_ptr((int64_t)x86_state->RDI);
			uint64_t length = x86_state->RSI;
			uint64_t prot = x86_state->RDX;
			uint64_t flags = x86_state->R10;
			uint64_t fd = x86_state->R8;
			uint64_t offset = x86_state->R9;

			if (flags & MAP_FIXED && (addr < (uintptr_t)get_memory_ptr(0) || (addr + length) > (uintptr_t)get_memory_ptr((off_t)memory_size_))) {
				// Prevent overwriting non-guest memory
				flags &= ~MAP_FIXED;
				flags |= MAP_FIXED_NOREPLACE;
			}

			uint64_t ptr = native_syscall(__NR_mmap, addr, length, prot, flags, fd, offset);
			if (!(ptr & (1ull << 63))) { // Positive return value (No error)
				ptr -= (uintptr_t)get_memory_ptr(0); // Adjust to guest space
				// TODO Negative pointer values possible (which might not be a good idea)
			}
			x86_state->RAX = ptr;

			break;
		}
		case 10: // mprotect
		{
			auto addr = (uintptr_t)get_memory_ptr((int64_t)x86_state->RDI);
			uint64_t length = x86_state->RSI;
			uint64_t prot = x86_state->RDX;

			auto ret = native_syscall(__NR_mprotect, addr, length, prot);
			x86_state->RAX = ret;
			break;
		}
		case 11: // munmap
		{
			auto addr = (uintptr_t)get_memory_ptr((int64_t)x86_state->RDI);
			uint64_t length = x86_state->RSI;

			// Don't allow arbitrary unmaps?
			x86_state->RAX = native_syscall(__NR_munmap, addr, length);

			break;
		}
		case 12: // brk
		{
			// 407bf7
			uint64_t addr = x86_state->RDI;
			if (addr == 0) {
				x86_state->RAX = brk_ - (uintptr_t)get_memory_ptr(0);
			} else if ((uintptr_t)get_memory_ptr((off_t)addr) < brk_) {
				brk_ = (uintptr_t)get_memory_ptr((off_t)addr);
				x86_state->RAX = addr;
			} else if ((uintptr_t)get_memory_ptr((off_t)addr) < brk_limit_) {

				uint64_t size = (uintptr_t)get_memory_ptr((off_t)addr) - brk_;
				uintptr_t aligned_ptr = brk_ & ~0xfffull;
				uintptr_t base_ptr_off = brk_ & 0xfffull;
				uintptr_t aligned_size = (size + base_ptr_off + 0xfff) & ~0xfffull;

				mprotect((void *)aligned_ptr, aligned_size, PROT_READ | PROT_WRITE);

				brk_ = (uintptr_t)get_memory_ptr((off_t)addr);

				x86_state->RAX = addr;
			} else {
				x86_state->RAX = brk_ - (uintptr_t)get_memory_ptr(0);
			}
			break;
		}
		case 14: // rt_sigprocmask
		{
			// Not sure if we should allow that
			auto set = (uintptr_t)get_memory_ptr(x86_state->RSI);
			auto oldset = (uintptr_t)get_memory_ptr(x86_state->RDX);

			auto ret = native_syscall(__NR_rt_sigprocmask, x86_state->RDI, set, oldset);
			x86_state->RAX = ret;
			break;
		}
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
		case 56: // clone
		{
			auto et = create_execution_thread();
			auto new_x86_state = (x86::x86_cpu_state *)et->get_cpu_state();
			memcpy(new_x86_state, x86_state, sizeof(*x86_state));

			new_x86_state->RAX = 0;

			pthread_t child;
			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_mutex_t rax_lock;
			pthread_cond_t rax_cond;
			pthread_mutex_init(&rax_lock, NULL);
			pthread_cond_init(&rax_cond, NULL);

			loop_args args = { new_x86_state, x86_state, &rax_lock, &rax_cond };
			pthread_mutex_lock(&rax_lock);

			pthread_create(&child, &attr, &MainLoopWrapper, &args);
			pthread_cond_wait(&rax_cond, &rax_lock);
			std::cerr << "Spawned thread " << x86_state->RAX << std::endl;

			pthread_detach(child);
			/*
			pthread_attr_t attr;
			if (pthread_getattr_np(pthread_self(), &attr) != 0)
				throw std::runtime_error("Failed to get current thread attr\n");

			auto current_frame = __builtin_frame_address(0);
			void *current_stack;
			size_t current_stack_size;
			if (pthread_attr_getstack(&attr, &current_stack, &current_stack_size) != 0)
				throw std::runtime_error("Failed to get current stack\n");
			pthread_attr_destroy(&attr);

			auto current_stack_end = (void *)0x7ffffffff000;
			current_stack_size = (uintptr_t)current_stack_end - (uintptr_t)current_stack;

			auto offset = ((uintptr_t)current_frame - 0x2b0) - (uintptr_t)current_stack;
			auto stack_ptr = mmap(NULL, current_stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK | MAP_GROWSDOWN, -1, 0);//(uintptr_t)get_memory_ptr(x86_state->RSI);
			if (!stack_ptr)
				throw std::runtime_error("Failed to allocate new stack\n");

			memcpy(stack_ptr, current_stack, current_stack_size);
			stack_ptr = (void *)((uintptr_t)stack_ptr + offset);	

			auto ptid_ptr = (uintptr_t)get_memory_ptr(x86_state->RDX);

			auto et = create_execution_thread();
			auto new_x86_state = et->get_cpu_state();
			memcpy(new_x86_state, x86_state, sizeof(*x86_state));

			std::cout << "Call from thread: " << gettid() << std::endl;

			register auto ctid_ptr __asm__("r10") = (uintptr_t)get_memory_ptr(x86_state->R10);
			register auto tls_ptr __asm__("r8") = (uintptr_t)get_memory_ptr(x86_state->R8);
			__asm__ volatile(
					"syscall\n"
					"test %%rax, %%rax\n"
					"cmove %6, %%rbp\n"
					: "+a"(ret)
					: "D"(flags), "S"((uint64_t)stack_ptr), "d"(ptid_ptr), "r"(ctid_ptr), "r"(tls_ptr), "r"((uintptr_t)stack_ptr + 0x2b0)
					: "memory", "rcx", "r11");
			std::cout << "Hello from thread: " << gettid() << std::endl;
			
			x86_state->RAX = ret;
			if (!ret) {
				// In reverse order to overwrite local x86_state last
				for (int i = ((uint64_t)current_stack_size/8)-1; i >= 0; i--) {
					if ( ((uint64_t *)stack_ptr)[i] == (uint64_t)cpu_state) {
						((uint64_t *)stack_ptr)[i] = (uint64_t)new_x86_state;
					}
				}
			}
			*/
			break;
		}
		case 77: // ftruncate
		{
			x86_state->RAX = native_syscall(__NR_ftruncate, x86_state->RDI, x86_state->RSI);
			break;
		}
		case 25: // mremap
		{
			// Hint to higher than already mapped memory if no hint
			auto old_addr = (uintptr_t)get_memory_ptr((int64_t)x86_state->RDI);
			uint64_t old_size = x86_state->RSI;
			uint64_t new_size = x86_state->RDX;
			uint64_t flags = x86_state->R10;
			auto new_addr = (uintptr_t)get_memory_ptr((int64_t)x86_state->R8);

			if (flags & MREMAP_FIXED && (new_addr < (uintptr_t)get_memory_ptr(0) || (new_addr + new_size) > (uintptr_t)get_memory_ptr((off_t)memory_size_))) {
				x86_state->RAX = -EINVAL;
				// IDK
				break;
			}

			uint64_t ptr = native_syscall(__NR_mremap, old_addr, old_size, new_size, flags, new_addr);
			if (!(ptr & (1ull << 63))) { // Positive return value (No error)
				ptr -= (uintptr_t)get_memory_ptr(0); // Adjust to guest space
				// TODO Negative pointer values possible (which might not be a good idea)
			}
			x86_state->RAX = ptr;

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
		case 202: // futex
		{
			auto addr = (uint64_t)get_memory_ptr(x86_state->RDI);
			auto timespec = (uint64_t)get_memory_ptr(x86_state->R10);
			auto addr2 = (uint64_t)get_memory_ptr(x86_state->R8);
			x86_state->RAX = native_syscall(__NR_futex, addr, x86_state->RSI, x86_state->RDX, timespec, addr2, x86_state->R9);
			break;
		}
		case 204: // sched_get_affinity
		{
			native_syscall(__NR_sched_getaffinity, x86_state->RDI, x86_state->RSI, (uintptr_t)get_memory_ptr((int64_t)x86_state->RDX));
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
		case 228: // clock_gettime
		{
			x86_state->RAX = native_syscall(__NR_clock_gettime, x86_state->RDI, (uintptr_t)get_memory_ptr((int64_t)x86_state->RSI));
			break;
		}
		case 60: //exit
		{
			native_syscall(__NR_exit, x86_state->RDI);
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
