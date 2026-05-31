#include <arancini/input/x86/cpuid.hpp>
#include <arancini/runtime/dbt/translation.h>
#include <arancini/runtime/exec/execution-context.h>
#include <arancini/runtime/exec/execution-thread.h>
#include <arancini/runtime/exec/native_syscall.h>
#include <arancini/runtime/exec/x86/x86-cpu-state.h>
#include <arancini/util/logger.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <pthread.h>
#include <sched.h>
#include <utility>

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
#include <iostream>
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
    util::global_logger.info("Thread: {}\nState:\n\t{}",
                             util::lazy_eval<>(gettid), *x86_state);
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

execution_context::execution_context(input::input_arch &ia,
                                     output::dynamic::dynamic_output_engine &oe,
                                     bool optimise)
    : memory_(nullptr), memory_size_(0x10000000ull), brk_{0},
      brk_limit_{UINTPTR_MAX}, te_(*this, ia, oe, optimise) {
    allocate_guest_memory();
    brk_ = reinterpret_cast<uintptr_t>(memory_);
    pthread_mutex_init(&big_fat_lock, NULL);
}

execution_context::~execution_context() {
    pthread_mutex_destroy(&big_fat_lock);
}

void *execution_context::add_memory_region(off_t base_address, size_t size,
                                           bool ignore_brk) {
    if ((base_address + size) > memory_size_) {
        throw std::runtime_error("memory region out of bounds");
    }

    uintptr_t base_ptr = (uintptr_t)memory_ + base_address;
    uintptr_t aligned_base_ptr = base_ptr & ~0xfffull;
    uintptr_t base_ptr_off = base_ptr & 0xfffull;
    uintptr_t aligned_size = (size + base_ptr_off + 0xfff) & ~0xfffull;

    util::global_logger.info("amr: base-pointer={:#x} aligned-base-ptr={:#x} "
                             "base-ptr-off={:#x} size={} aligned-size={}\n",
                             base_ptr, aligned_base_ptr, base_ptr_off, size,
                             aligned_size);

    mprotect((void *)aligned_base_ptr, aligned_size, PROT_READ | PROT_WRITE);

    if (!ignore_brk) {
        brk_ = std::max(aligned_base_ptr + aligned_size, brk_);
    } else {
        brk_limit_ = std::min(brk_limit_, aligned_base_ptr);
    }

    return (void *)base_ptr;
}

void execution_context::allocate_guest_memory() {
    memory_ = mmap(GM_BASE, memory_size_, PROT_NONE,
                   MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (memory_ == MAP_FAILED) {
        throw std::runtime_error("Unable to allocate guest memory (" +
                                 std::to_string(errno) + ")");
    }

#if defined(ARCH_X86_64)
    // The GS register is used as the base address for the emulated guest
    // memory.  Static and dynamic code generate memory instructions based on
    // this.
    syscall(SYS_arch_prctl, ARCH_SET_GS, (unsigned long long)memory_);
#endif
}

void arancini::runtime::exec::execution_context::handle_cpuid(
    x86::x86_cpu_state *x86_state) {
    std::pair<uint32_t, uint32_t> cpuid_input{util::copy(x86_state->RAX),
                                              util::copy(x86_state->RCX)};
    auto result = input::x86::cpuid_map.find(cpuid_input);

    if (result != input::x86::cpuid_map.end()) {
        auto cpuid_result = result->second;
        x86_state->RAX = cpuid_result->eax;
        x86_state->RBX = cpuid_result->ebx;
        x86_state->RCX = cpuid_result->ecx;
        x86_state->RDX = cpuid_result->edx;
        util::global_logger.info(
            "CPUID: {:#x} {:#x} -> {:#x} {:#x} {:#x} {:#x}\n",
            cpuid_input.first, cpuid_input.second, util::copy(x86_state->RAX),
            util::copy(x86_state->RBX), util::copy(x86_state->RCX),
            util::copy(x86_state->RDX));
    } else {
        util::global_logger.error("CPUID: {:#x} {:#x} not found\n",
                                  cpuid_input.first, cpuid_input.second);
        x86_state->RAX = 0;
        x86_state->RBX = 0;
        x86_state->RCX = 0;
        x86_state->RDX = 0;
    }
}

std::shared_ptr<execution_thread> execution_context::create_execution_thread() {
    auto et =
        std::make_shared<execution_thread>(*this, sizeof(x86::x86_cpu_state));
    threads_[et->get_cpu_state()] = et;

    return et;
}

int execution_context::invoke(void *cpu_state) {
    if (!cpu_state)
        throw std::invalid_argument("invoke() received null CPU state");

    auto et = threads_[cpu_state];
    if (!et)
        throw std::runtime_error("unable to resolve execution thread");

    auto x86_state = (x86::x86_cpu_state *)cpu_state;

    util::global_logger.debug("{}\n", util::logging_separator('='))
        .debug("INVOKE PC = {:#x}\n", util::copy(x86_state->PC))
        .debug("{}\n", util::logging_separator('='));
    util::global_logger.debug("Registers:\n{}\n", *x86_state)
        .debug("{}\n", util::logging_separator('-'));
    // util::global_logger.debug("STACK:\n").
    //                    debug("{}\n", util::logging_separator('-'));
    // auto* memptr = reinterpret_cast<uint64_t*>(get_memory_ptr(0)) +
    // x86_state->RSP; x86::print_stack(std::cerr, memptr, 20);

    pthread_mutex_lock(&big_fat_lock);
    auto txln = te_.get_translation(x86_state->PC);
    if (txln == nullptr) {
        util::global_logger.error("Unable to translate\n");
        pthread_mutex_unlock(&big_fat_lock);
        return 1;
    }

    // Chain
    if (et->chain_address_) {
        util::global_logger.debug("Chaining previous block to {:#x}\n",
                                  util::copy(x86_state->PC));

        te_.chain(et->chain_address_, txln->get_code_ptr());
    }

    pthread_mutex_unlock(&big_fat_lock);
    const dbt::native_call_result result = txln->invoke(cpu_state);

    et->chain_address_ = result.chain_address;

    return result.exit_code;
}

// calling convention for syscalls on x64 has params in:
// "%rdi, %rsi, %rdx, %r10, %r8 and %r9"

#define SYSCALL_PARAM0
#define SYSCALL_PARAM1 , x86_state->RDI
#define SYSCALL_PARAM2 SYSCALL_PARAM1, x86_state->RSI
#define SYSCALL_PARAM3 SYSCALL_PARAM2, x86_state->RDX
#define SYSCALL_PARAM4 SYSCALL_PARAM3, x86_state->R10
#define SYSCALL_PARAM5 SYSCALL_PARAM4, x86_state->R8
#define SYSCALL_PARAM6 SYSCALL_PARAM5, x86_state->R9
#define SYSCALL_PARAMS(n) SYSCALL_PARAM##n
#define SIMPLE_SYSCALL(x86_num, name, num_params)                              \
    case x86_num: {                                                            \
        util::global_logger.debug("System call: " #name "()\n");               \
        auto ret = native_syscall(__NR_##name SYSCALL_PARAMS(num_params));     \
        x86_state->RAX = ret;                                                  \
        break;                                                                 \
    }
#define COMPLEX_SYSCALL(x86_num, name, ...)                                    \
    case x86_num: {                                                            \
        util::global_logger.debug("System call: " #name "()\n");               \
        auto ret = native_syscall(__NR_##name, __VA_ARGS__);                   \
        x86_state->RAX = ret;                                                  \
        break;                                                                 \
    }

int execution_context::internal_call(void *cpu_state, int call) {
    if (call == 1) { // syscall
        auto x86_state = (x86::x86_cpu_state *)cpu_state;
        util::global_logger.debug("System call number: {}\n",
                                  util::copy(x86_state->RAX));
        switch (x86_state->RAX) {
            SIMPLE_SYSCALL(0, read, 3);
            SIMPLE_SYSCALL(1, write, 3);
            COMPLEX_SYSCALL(2, openat,
                            (unsigned long)AT_FDCWD SYSCALL_PARAMS(3));
            SIMPLE_SYSCALL(3, close, 1);
            SIMPLE_SYSCALL(8, lseek, 3);
            SIMPLE_SYSCALL(10, mprotect, 3);
            SIMPLE_SYSCALL(11, munmap, 2);
            SIMPLE_SYSCALL(14, rt_sigprocmask, 4);
            SIMPLE_SYSCALL(19, readv, 3);
            SIMPLE_SYSCALL(20, writev, 3);
            COMPLEX_SYSCALL(21, faccessat,
                            (unsigned long)AT_FDCWD SYSCALL_PARAMS(2));
            SIMPLE_SYSCALL(28, madvise, 3);
            SIMPLE_SYSCALL(35, nanosleep, 2);
            SIMPLE_SYSCALL(39, getpid, 0);
            SIMPLE_SYSCALL(41, socket, 3);
            SIMPLE_SYSCALL(42, connect, 3);
            SIMPLE_SYSCALL(43, accept, 3);
            SIMPLE_SYSCALL(44, sendto, 6);
            SIMPLE_SYSCALL(45, recvfrom, 6);
            SIMPLE_SYSCALL(46, sendmsg, 3);
            SIMPLE_SYSCALL(47, recvmsg, 3);
            SIMPLE_SYSCALL(48, shutdown, 2);
            SIMPLE_SYSCALL(49, bind, 3);
            SIMPLE_SYSCALL(50, listen, 2);
            SIMPLE_SYSCALL(51, getsockname, 3);
            SIMPLE_SYSCALL(52, getpeername, 3);
            SIMPLE_SYSCALL(53, socketpair, 4);
            SIMPLE_SYSCALL(54, setsockopt, 5);
            SIMPLE_SYSCALL(60, exit, 1);
            SIMPLE_SYSCALL(63, uname, 1);
            SIMPLE_SYSCALL(72, fcntl, 3);
            SIMPLE_SYSCALL(77, ftruncate, 2);
            COMPLEX_SYSCALL(83, mkdirat,
                            (unsigned long)AT_FDCWD SYSCALL_PARAMS(2));
            SIMPLE_SYSCALL(96, gettimeofday, 2);
            SIMPLE_SYSCALL(186, gettid, 0);
            SIMPLE_SYSCALL(200, tkill, 2);
            SIMPLE_SYSCALL(202, futex, 6);
            SIMPLE_SYSCALL(203, sched_setaffinity, 3);
            SIMPLE_SYSCALL(204, sched_getaffinity, 3);
            SIMPLE_SYSCALL(231, exit_group, 1);
            SIMPLE_SYSCALL(228, clock_gettime, 2);
            SIMPLE_SYSCALL(230, clock_nanosleep, 4);
            SIMPLE_SYSCALL(234, tgkill, 3);
            SIMPLE_SYSCALL(257, openat, 4);
            SIMPLE_SYSCALL(258, mkdirat, 3);
            SIMPLE_SYSCALL(262, newfstatat, 4)
            SIMPLE_SYSCALL(267, readlinkat, 4);
            SIMPLE_SYSCALL(273, set_robust_list, 2);
            SIMPLE_SYSCALL(318, getrandom, 3);
            SIMPLE_SYSCALL(324, membarrier, 2);
            SIMPLE_SYSCALL(334, rseq, 2);
            SIMPLE_SYSCALL(302, prlimit64, 2);
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
            struct stat tmp_struct{};

            uint64_t result;
            if (x86_state->RAX == 6) {
                result = native_syscall(
                    __NR_newfstatat, (unsigned long)AT_FDCWD, (uintptr_t)name,
                    (uintptr_t)&tmp_struct, (unsigned long)AT_SYMLINK_NOFOLLOW);
            } else if (x86_state->RAX == 4) {
                result = native_syscall(
                    __NR_newfstatat, (unsigned long)AT_FDCWD, (uintptr_t)name,
                    (uintptr_t)&tmp_struct, 0ul);
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
                } __attribute__((packed)) *target =
                    (struct target_stat *)get_memory_ptr((off_t)statp);

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
            ts.tv_sec = (long)(msec / 1000);
            ts.tv_nsec = (msec % 1000) * 1000000;
            auto ret =
                native_syscall(__NR_ppoll, ptr, x86_state->RSI, (uintptr_t)&ts,
                               (uintptr_t)NULL, sizeof(sigset_t));
            x86_state->RAX = ret;
            break;
        }
        case 9: // mmap
        {
            util::global_logger.debug("System call: mmap()\n");

            // Hint to higher than already mapped memory if no hint
            auto addr =
                x86_state->RDI == 0
                    ? (uintptr_t)memory_ + (off_t)memory_size_ + 4096
                    : (uintptr_t)get_memory_ptr((int64_t)x86_state->RDI);
            uint64_t length = x86_state->RSI;
            uint64_t prot = x86_state->RDX;
            uint64_t flags = x86_state->R10;
            uint64_t fd = x86_state->R8;
            uint64_t offset = x86_state->R9;

            if (flags & MAP_FIXED &&
                (addr < (uintptr_t)memory_ ||
                 (addr + length) > (uintptr_t)memory_ + memory_size_)) {
                // Prevent overwriting non-guest memory
                flags &= ~MAP_FIXED;
                flags |= MAP_FIXED_NOREPLACE;
            }

            uint64_t ptr = native_syscall(__NR_mmap, addr, length, prot, flags,
                                          fd, offset);
            if (!(ptr & (1ull << 63))) { // Positive return value (No error)
                ptr -= (uintptr_t)get_memory_ptr(0); // Adjust to guest space
                // TODO Negative pointer values possible (which might not be a
                // good idea)
            }
            x86_state->RAX = ptr;

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
                uintptr_t aligned_size =
                    (size + base_ptr_off + 0xfff) & ~0xfffull;

                mprotect((void *)aligned_ptr, aligned_size,
                         PROT_READ | PROT_WRITE);

                brk_ = (uintptr_t)get_memory_ptr((off_t)addr);

                x86_state->RAX = addr;
            } else {
                x86_state->RAX = brk_ - (uintptr_t)get_memory_ptr(0);
            }
            break;
        }
        case 16: // ioctl
        {
            uint64_t fd = x86_state->RDI;
            switch (fd) {
            case STDIN_FILENO:
            case STDOUT_FILENO:
            case STDERR_FILENO:
                break;
            default:
                util::global_logger.error(
                    "Unsupported file descriptor for ioctl: {}\n", fd);
                x86_state->RAX = -EBADF;
                return 1;
            }

            x86_state->RAX =
                native_syscall(__NR_ioctl, fd, x86_state->RSI, x86_state->RDX,
                               x86_state->R10, x86_state->R8, x86_state->R9);
            break;
        }
        case 56: // clone
        {
            util::global_logger.debug("System call: clone()\n");

            auto et = create_execution_thread();
            auto new_x86_state = (x86::x86_cpu_state *)et->get_cpu_state();
            util::global_logger.debug("New CPU state: {:#x}\n",
                                      (uintptr_t)new_x86_state);
            memcpy(new_x86_state, x86_state, sizeof(*x86_state));

            new_x86_state->RAX = 0;

            pthread_t child;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_mutex_t rax_lock;
            pthread_cond_t rax_cond;
            pthread_mutex_init(&rax_lock, NULL);
            pthread_cond_init(&rax_cond, NULL);

            loop_args args = {new_x86_state, x86_state, &rax_lock, &rax_cond,
                              (uintptr_t)get_memory_ptr(0)};
            pthread_mutex_lock(&rax_lock);

            pthread_create(&child, &attr, &MainLoopWrapper, &args);
            pthread_cond_wait(&rax_cond, &rax_lock);

            pthread_mutex_unlock(&rax_lock);
            pthread_mutex_destroy(&rax_lock);
            pthread_cond_destroy(&rax_cond);
            // pthread_detach(child);
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

            if (flags & MREMAP_FIXED &&
                (new_addr < (uintptr_t)memory_ ||
                 new_addr + new_size > (uintptr_t)memory_ + memory_size_)) {
                x86_state->RAX = -EINVAL;
                // IDK
                break;
            }

            uint64_t ptr = native_syscall(__NR_mremap, old_addr, old_size,
                                          new_size, flags, new_addr);
            if (!(ptr & (1ull << 63))) { // Positive return value (No error)
                ptr -= (uintptr_t)get_memory_ptr(0); // Adjust to guest space
                // TODO Negative pointer values possible (which might not be a
                // good idea)
            }
            x86_state->RAX = ptr;

            break;
        }
        case 158:                     // arch_prctl
            util::global_logger.debug("System call: arch_prctl()");
            switch (x86_state->RDI) { // code
            case 0x1001:              // ARCH_SET_GS
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
            x86_state->R11 = 0x246;
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
        default:
            util::global_logger.error("Unsupported system call: {:#x}\n",
                                      util::copy(x86_state->RAX));
            return 1;
        }
    } else if (call == 3) {
        auto x86_state = (x86::x86_cpu_state *)cpu_state;
        auto pc = x86_state->PC;
        util::global_logger.error("Poison Instr @ GuestPC: {:#x}", pc);
        abort();
    } else if (call == 4) {
        handle_cpuid((x86::x86_cpu_state *)cpu_state);
        return 0;
    } else {
        util::global_logger.error("Unsupported internal call: {}", call);
        return 1;
    }
    return 0;
}
std::shared_ptr<execution_thread>
execution_context::get_thread(void *cpu_state) {
    return threads_.at(cpu_state);
}

std::pair<decltype(execution_context::threads_)::const_iterator,
          decltype(execution_context::threads_)::const_iterator>
execution_context::get_thread_range() {
    return std::make_pair(threads_.cbegin(), threads_.cend());
}
