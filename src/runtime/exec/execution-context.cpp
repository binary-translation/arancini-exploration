#include <arancini/runtime/dbt/translation.h>
#include <arancini/runtime/exec/execution-context.h>
#include <arancini/runtime/exec/execution-thread.h>
#include <arancini/runtime/exec/native_syscall.h>
#include <arancini/runtime/exec/x86/x86-cpu-state.h>
#include <arancini/util/logger.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <pthread.h>
#include <sched.h>

#include <sys/syscall.h>
#if defined(ARCH_X86_64)
#include <asm/prctl.h>
#include <unistd.h>
#define GM_BASE (void *)0x600000000000ull
#else
#define GM_BASE nullptr
#endif

#include <asm-generic/ioctls.h>
#include <asm/stat.h>
#include <csignal>
#include <linux/fcntl.h>
#include <linux/futex.h>
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
	uintptr_t mem_base;
};

void *MainLoopWrapper(void *args) {
	auto largs = (loop_args *)args;
	auto x86_state = (x86::x86_cpu_state *)largs->new_state;
	auto parent_state = (x86::x86_cpu_state *)largs->parent_state;
    auto flags = parent_state->RDI;

	pthread_mutex_lock(largs->lock);
    util::global_logger.info("Thread: {}\nState:\n\t{}", util::lazy_eval<>(gettid), *x86_state);
	parent_state->RAX = gettid();
	x86_state->RSP = x86_state->RSI;
	x86_state->FS = x86_state->R8;

    int *ctid = (int *)parent_state->R10;
    if (flags & CLONE_PARENT_SETTID) {
        *(int *)parent_state->RDX = gettid();
    }
    if (flags & CLONE_CHILD_CLEARTID) {
        syscall(SYS_set_tid_address, ctid);
    }

	pthread_cond_signal(largs->cond);
	pthread_mutex_unlock(largs->lock);

	MainLoop(x86_state);
    syscall(SYS_futex, ctid, FUTEX_WAKE, 1, NULL, NULL, 0);
	return NULL;
};

execution_context::execution_context(input::input_arch &ia, output::dynamic::dynamic_output_engine &oe, bool optimise)
	: memory_(nullptr)
	, memory_size_(0x10000000ull)
	, brk_ { 0 }
	, brk_limit_ { UINTPTR_MAX }
	, te_(*this, ia, oe, optimise)
{
	allocate_guest_memory();
	brk_ = reinterpret_cast<uintptr_t>(memory_);
	pthread_mutex_init(&big_fat_lock, NULL);
}

execution_context::~execution_context() {
	pthread_mutex_destroy(&big_fat_lock);
}

void *execution_context::add_memory_region(off_t base_address, size_t size, bool ignore_brk) {
    [[unlikely]]
	if ((base_address + size) > memory_size_) {
		throw std::runtime_error("memory region out of bounds");
	}

    std::uintptr_t base_ptr = (uintptr_t)memory_ + base_address;
	std::uintptr_t aligned_base_ptr = base_ptr & ~0xfffull;
	std::uintptr_t base_ptr_off = base_ptr & 0xfffull;
	std::uintptr_t aligned_size = (size + base_ptr_off + 0xfff) & ~0xfffull;

    util::global_logger.info("amr: base-pointer={:#x} aligned-base-ptr={:#x} base-ptr-off={:#x} size={} aligned-size={}\n",
                             base_ptr, aligned_base_ptr, base_ptr_off, size, aligned_size);

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
		throw std::runtime_error("Unable to allocate guest memory (" + std::to_string(errno) + ")");
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

int execution_context::invoke(void *cpu_state) {
    [[unlikely]]
    if (!cpu_state) throw std::invalid_argument("invoke() received null CPU state");

	auto et = threads_[cpu_state];

    [[unlikely]]
	if (!et) throw std::runtime_error("unable to resolve execution thread");

	auto x86_state = (x86::x86_cpu_state *)cpu_state;
    util::global_logger.info("{}\n", util::logging_separator('='))
                       .info("INVOKE PC = {:#x}\n", util::copy(x86_state->PC))
                       .info("{}\n", util::logging_separator('='));
    util::global_logger.info("Registers:\n{}\n", *x86_state)
                       .info("{}\n", util::logging_separator('-'));
    // util::global_logger.debug("STACK:\n").
    //                    debug("{}\n", util::logging_separator('-'));
    //auto* memptr = reinterpret_cast<uint64_t*>(get_memory_ptr(0)) + x86_state->RSP;
    //x86::print_stack(std::cerr, memptr, 20);

	pthread_mutex_lock(&big_fat_lock);

	auto txln = te_.get_translation(x86_state->PC);

    [[unlikely]]
	if (txln == nullptr) {
        util::global_logger.error("Unable to translate\n");
		pthread_mutex_unlock(&big_fat_lock);
		return 1;
	}

	// Chain
	if (util::system_config::get().is_chaining() && et->chain_address_) {
        util::global_logger.info("Chaining previous block to {:#x}\n", util::copy(x86_state->PC));
		te_.chain(et->chain_address_, txln->get_code_ptr());
	}

	pthread_mutex_unlock(&big_fat_lock);
	const dbt::native_call_result result = txln->invoke(cpu_state);

	et->chain_address_ = result.chain_address;

	return result.exit_code;
}

int execution_context::internal_call(void *cpu_state, int call) {
    [[likely]]
	if (call == 1) { // syscall
		auto x86_state = (x86::x86_cpu_state *)cpu_state;
        util::global_logger.debug("System call number: {}\n", util::copy(x86_state->RAX));

        x86_state->RCX = x86_state->PC;
        x86_state->R11 = 0x202 | ((x86_state->ZF & 0x1) << 6) | x86_state->CF |
                                 ((x86_state->SF & 0x1) << 7) | ((x86_state->OF & 0x1) << 11);
		switch (x86_state->RAX) {
		case 0: // read
		{
			util::global_logger.debug("System call: read()\n");
			uint64_t fd = x86_state->RDI;
			auto buf = (uintptr_t)get_memory_ptr((off64_t)x86_state->RSI);
			uint64_t count = x86_state->RDX;
			x86_state->RAX = native_syscall(__NR_read, fd, buf, count);
			break;
		}
		case 1: // write
		{
            util::global_logger.debug("System call: write()\n");
			uint64_t fd = x86_state->RDI;
			auto buf = (uintptr_t)get_memory_ptr((off64_t)x86_state->RSI);
			uint64_t count = x86_state->RDX;
			x86_state->RAX = native_syscall(__NR_write, fd, buf, count);
			break;
		}
		case 2: // open
		{
            off64_t offset = x86_state->RDI;
            auto filename = reinterpret_cast<std::uintptr_t>(get_memory_ptr(offset));
            std::uint64_t flags = x86_state->RSI;
			std::uint64_t mode = x86_state->RDX;
            std::uint64_t dirfd = AT_FDCWD;

            // Check for O_LARGEFILE
            // It's mapped to a different numeric code on Aarch64
            if (0x8000 & flags)
                flags = (flags ^ 0x8000) | O_LARGEFILE;

            util::global_logger.debug("System call: openat({:#x}, {}, {:#x}, {})\n",
                                      dirfd, reinterpret_cast<const char*>(filename), flags, mode);

			x86_state->RAX = native_syscall(__NR_openat, dirfd, filename, flags, mode);
			break;
		}
		case 3: // close
            util::global_logger.debug("System call: close()\n");
			x86_state->RAX = native_syscall(__NR_close, x86_state->RDI);
			break;
		case 4: // stat
		case 5: // fstat
		case 6: // lstat
		{
			if (x86_state->RAX == 4) {
				util::global_logger.debug("System call: stat()\n");
			} else if (x86_state->RAX == 5) {
				util::global_logger.debug("System call: fstat()\n");
			} else {
				util::global_logger.debug("System call: lstat()\n");
			}
			uint64_t fd = x86_state->RDI;
			auto name = (uintptr_t)get_memory_ptr((off_t)x86_state->RDI);

			uint64_t statp = x86_state->RSI;
			struct stat tmp_struct { };

			uint64_t result;
			if (x86_state->RAX == 6) {
				result = native_syscall(__NR_newfstatat, (unsigned long)AT_FDCWD, (uintptr_t)name, (uintptr_t)&tmp_struct, (unsigned long)AT_SYMLINK_NOFOLLOW);
			} else if (x86_state->RAX == 4) {
				result = native_syscall(__NR_newfstatat, (unsigned long)AT_FDCWD, (uintptr_t)name, (uintptr_t)&tmp_struct, 0ul);
			} else {
				result = native_syscall(__NR_fstat, fd, (uintptr_t)&tmp_struct);
			}
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
		case 7: // poll
		{
			// AARCH64 doesn't have poll, use ppoll instead
			auto ptr = (uintptr_t)get_memory_ptr(x86_state->RDI);
			struct timespec ts;
			auto msec = x86_state->RDX;
			ts.tv_sec = (long)(msec/1000);
			ts.tv_nsec = (msec%1000)*1000000;
			auto ret = native_syscall(__NR_ppoll, ptr, x86_state->RSI, (uintptr_t)&ts, (uintptr_t)NULL, sizeof(sigset_t));
			x86_state->RAX = ret;
			break;
		}
		case 8: // lseek
		{
			util::global_logger.debug("System call: lseek()\n");
			uint64_t fd = x86_state->RDI;
			uint64_t offset = x86_state->RSI;
			uint64_t whence = x86_state->RDX;
			x86_state->RAX = native_syscall(__NR_lseek, fd, offset, whence);
			break;
		}
		case 9: // mmap
		{
           util::global_logger.debug("System call: mmap()\n");

			// Hint to higher than already mapped memory if no hint
			auto addr = x86_state->RDI == 0 ? (uintptr_t)memory_ + (off_t)memory_size_ + 4096 : (uintptr_t)get_memory_ptr((int64_t)x86_state->RDI);
			uint64_t length = x86_state->RSI;
			uint64_t prot = x86_state->RDX;
			uint64_t flags = x86_state->R10;
			uint64_t fd = x86_state->R8;
			uint64_t offset = x86_state->R9;

			if (flags & MAP_FIXED && (addr < (uintptr_t)memory_ || (addr + length) > (uintptr_t)memory_ + memory_size_)) {
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
           util::global_logger.debug("System call: mprotect()\n");

			auto addr = (uintptr_t)get_memory_ptr((int64_t)x86_state->RDI);
			uint64_t length = x86_state->RSI;
			uint64_t prot = x86_state->RDX;

			auto ret = native_syscall(__NR_mprotect, addr, length, prot);
			x86_state->RAX = ret;
			break;
        }
		case 11: // munmap
        {
            util::global_logger.debug("System call: munmap()\n");

			auto addr = (uintptr_t)get_memory_ptr((int64_t)x86_state->RDI);
			uint64_t length = x86_state->RSI;

			// Don't allow arbitrary unmaps?
			x86_state->RAX = native_syscall(__NR_munmap, addr, length);

			break;
        }
		case 12: // brk
        {
            util::global_logger.debug("System call: brk()\n");

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
        case 13: // rt_sigaction
        {
            util::global_logger.debug("System call: rt_sigaction()\n");

            // TODO:
            // 1. Block SIGSEGV (but store the handler; our SIGSEGV handler should catch the SIGSEGV
            // instead and then maybe forward it)
            // 2. The oldset should write nullptr when no RDX is given; rather than 0
            // 3. native_syscall() expects std::uint64_t, but we're passing it std::uintptr_t
            auto act = reinterpret_cast<std::uintptr_t>(get_memory_ptr(x86_state->RSI));
            auto oldact = x86_state->RDX ? reinterpret_cast<std::uintptr_t>(x86_state->RDX) : 0;

            x86_state->RAX = native_syscall(__NR_rt_sigaction, x86_state->RDI, act, oldact, x86_state->R10);
            break;
        }
		case 14: // rt_sigprocmask
        {
            util::global_logger.debug("System call: rt_sigprocmask()\n");

			// Not sure if we should allow that
			auto set = (uintptr_t)get_memory_ptr(x86_state->RSI);
			auto oldset = x86_state->RDX ? (uintptr_t)get_memory_ptr(x86_state->RDX) : 0;

			x86_state->RAX = native_syscall(__NR_rt_sigprocmask, x86_state->RDI, set, oldset, x86_state->R10);
			break;
        }
		case 16: // ioctl
        {
            util::global_logger.debug("System call: ioctl()\n");

			// Not sure how many actually needed

			uint64_t arg = x86_state->RDX;
			uint64_t request = x86_state->RSI;
			switch (request) {
			case TIOCGWINSZ:
				arg = (uintptr_t)get_memory_ptr(arg);
				break;
			default:
                util::global_logger.warn("Unknown ioctl request {}\n", request);
				x86_state->RAX = -EINVAL;
				return 0;
			}

			x86_state->RAX = native_syscall(__NR_ioctl, x86_state->RDI, request, arg);
			break;
        }
		case 19: // readv
		{
			util::global_logger.debug("System call: readv()\n");

			auto iovec = (const struct iovec *)get_memory_ptr(x86_state->RSI);
			auto iocnt = x86_state->RDX;
			struct iovec iovec_new[iocnt];
			for (auto i = 0ull; i < iocnt; ++i) {
				iovec_new[i].iov_base = reinterpret_cast<void *>(get_memory_ptr(((uintptr_t)iovec[i].iov_base)));
				iovec_new[i].iov_len = iovec[i].iov_len;
			}

			x86_state->RAX = native_syscall(__NR_readv, x86_state->RDI, (uintptr_t)iovec_new, iocnt);
			break;
		}
		case 20: // writev
        {
            util::global_logger.debug("System call: writev()\n");

			auto iovec = (const struct iovec *)get_memory_ptr(x86_state->RSI);
			auto iocnt = x86_state->RDX;
			struct iovec iovec_new[iocnt];
			for (auto i = 0ull; i < iocnt; ++i) {
				iovec_new[i].iov_base = reinterpret_cast<void *>(get_memory_ptr(((uintptr_t)iovec[i].iov_base)));
				iovec_new[i].iov_len = iovec[i].iov_len;
			}

			x86_state->RAX = native_syscall(__NR_writev, x86_state->RDI, (uintptr_t)iovec_new, iocnt);
			break;
        }
        case 21: // access
            x86_state->RAX = native_syscall(__NR_faccessat, static_cast<std::uint64_t>(AT_FDCWD),
                                            x86_state->RDI, x86_state->RSI);
            break;
		case 28: // madvise
        {
            util::global_logger.debug("System call: madvise()\n");
			auto start = (uintptr_t)get_memory_ptr(x86_state->RDI);
			x86_state->RAX = native_syscall(__NR_madvise, start, x86_state->RSI, x86_state->RDX);
			break;
        }
		case 56: // clone
        {
           util::global_logger.debug("System call: clone()\n");

			auto et = create_execution_thread();
			auto new_x86_state = (x86::x86_cpu_state *)et->get_cpu_state();
			util::global_logger.debug("New CPU state: {:#x}\n", (uintptr_t)new_x86_state);
			memcpy(new_x86_state, x86_state, sizeof(*x86_state));

			new_x86_state->RAX = 0;

			pthread_t child;
			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_mutex_t rax_lock;
			pthread_cond_t rax_cond;
			pthread_mutex_init(&rax_lock, NULL);
			pthread_cond_init(&rax_cond, NULL);

			loop_args args = { new_x86_state, x86_state, &rax_lock, &rax_cond, (uintptr_t)get_memory_ptr(0) };
			pthread_mutex_lock(&rax_lock);

			pthread_create(&child, &attr, &MainLoopWrapper, &args);
			pthread_cond_wait(&rax_cond, &rax_lock);

			pthread_mutex_unlock(&rax_lock);
			pthread_mutex_destroy(&rax_lock);
			pthread_cond_destroy(&rax_cond);
			//pthread_detach(child);
			break;
        }
		case 77: // ftruncate
        {
            util::global_logger.debug("System call: ftruncate()\n");
			x86_state->RAX = native_syscall(__NR_ftruncate, x86_state->RDI, x86_state->RSI);
			break;
        }
		case 25: // mremap
        {
            util::global_logger.debug("System call: mremap()\n");

			// Hint to higher than already mapped memory if no hint
			auto old_addr = (uintptr_t)get_memory_ptr((int64_t)x86_state->RDI);
			uint64_t old_size = x86_state->RSI;
			uint64_t new_size = x86_state->RDX;
			uint64_t flags = x86_state->R10;
			auto new_addr = (uintptr_t)get_memory_ptr((int64_t)x86_state->R8);

			if (flags & MREMAP_FIXED && (new_addr < (uintptr_t)memory_ || new_addr + new_size > (uintptr_t)memory_ + memory_size_)) {
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
            util::global_logger.debug("System call: arch_prctl()");
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
				*(uint64_t *)get_memory_ptr(x86_state->RSI) = x86_state->FS;
				x86_state->RAX = 0;
				break;
			case 0x1004: // ARCH_GET_GS
				*(uint64_t *)get_memory_ptr(x86_state->RSI) = x86_state->GS;
				x86_state->RAX = 0;
				break;
			default:
				x86_state->RAX = -EINVAL;
			}
            x86_state->R11=0x246;
			break;
		case 186: // gettid
			util::global_logger.debug("System call: gettid()\n");
			x86_state->RAX = gettid();
			break;
		case 200: // tkill
            util::global_logger.debug("System call: kill()\n");
			x86_state->RAX = native_syscall(__NR_tkill, x86_state->RDI, x86_state->RSI);
			break;
		case 202: // futex
        {
            util::global_logger.debug("System call: futex()\n");
			auto addr = (uint64_t)get_memory_ptr(x86_state->RDI);
			auto timespec = x86_state->R10 ? (uint64_t)get_memory_ptr(x86_state->R10) : 0;
			auto addr2 = (uint64_t)get_memory_ptr(x86_state->R8);
			x86_state->RAX = native_syscall(__NR_futex, addr, x86_state->RSI, (uint64_t)x86_state->RDX, timespec, addr2, x86_state->R9);
			break;
        }
		case 203: // sched_set_affinity
            util::global_logger.debug("System call: sched_set_affinity()\n");
			x86_state->RAX = native_syscall(__NR_sched_setaffinity, x86_state->RDI, x86_state->RSI, (uintptr_t)get_memory_ptr((int64_t)x86_state->RDX));
			break;
		case 204: // sched_get_affinity
            util::global_logger.debug("System call: sched_get_affinity()\n");
			x86_state->RAX = native_syscall(__NR_sched_getaffinity, x86_state->RDI, x86_state->RSI, (uintptr_t)get_memory_ptr((int64_t)x86_state->RDX));
			break;
		case 218: // set_tid_address
        {
            util::global_logger.debug("System call: set_tid_address()\n");

			// TODO Handle clear_child_tid in exit
			auto et = threads_[cpu_state];
			et->clear_child_tid_ = (int *)get_memory_ptr(x86_state->RDI);
			x86_state->RAX = gettid();
			break;
		}
		case 231:
            util::global_logger.debug("System call: exit()\n");

            util::global_logger.info("Exiting from emulated process with exit code: {}\n", util::copy(x86_state->RDI));
			exit(x86_state->RDI);
			return 1;
		case 228: // clock_gettime
            util::global_logger.debug("System call: clock_gettime()\n");
			x86_state->RAX = native_syscall(__NR_clock_gettime, x86_state->RDI, (uintptr_t)get_memory_ptr((int64_t)x86_state->RSI));
			break;
		case 60: //exit
            util::global_logger.debug("System call: exit()\n");
			native_syscall(__NR_exit, x86_state->RDI);
            break;
		case 324: //membarrier
			util::global_logger.debug("System call: membarrier()\n");
			native_syscall(__NR_rseq, x86_state->RDI, x86_state->RSI);
			break;
		default:
            util::global_logger.error("Unsupported system call: {:#x}\n", util::copy(x86_state->RAX));
			return 1;
		}
	} else if (call == 3) {
		auto x86_state = (x86::x86_cpu_state *)cpu_state;
		auto pc = x86_state->PC;
        util::global_logger.error("Poison Instr @ GuestPC: {:#x}", pc);
		abort();
	}else {
        util::global_logger.error("Unsupported internal call: {}", call);
		return 1;
	}

	return 0;
}

[[nodiscard]]
std::shared_ptr<execution_thread> execution_context::get_thread(void *cpu_state) {
    return threads_.at(cpu_state);
}

[[nodiscard]]
std::pair<decltype(execution_context::threads_)::const_iterator, decltype(execution_context::threads_)::const_iterator> execution_context::get_thread_range() {
        return std::make_pair(threads_.cbegin(), threads_.cend());
}

