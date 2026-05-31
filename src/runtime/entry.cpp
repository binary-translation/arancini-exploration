#include <arancini/runtime/exec/execution-context.h>
#include <arancini/runtime/exec/execution-thread.h>
#include <arancini/runtime/exec/guest_support.h>
#include <arancini/runtime/exec/x86/x86-cpu-state.h>
#include <arancini/util/logger.h>
#include <cstdint>
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
#ifndef NDEBUG
static arancini::input::x86::x86_input_arch
    ia(true, arancini::input::x86::disassembly_syntax::intel);
#else
static arancini::input::x86::x86_input_arch
    ia(false, arancini::input::x86::disassembly_syntax::intel);
#endif

#if defined(ARCH_X86_64)
static arancini::output::dynamic::x86::x86_dynamic_output_engine oe;
#elif defined(ARCH_AARCH64)
static arancini::output::dynamic::arm64::arm64_dynamic_output_engine oe;
#elif defined(ARCH_RISCV64)
static arancini::output::dynamic::riscv64::riscv64_dynamic_output_engine oe;
#else
#error "Unsupported dynamic output architecture"
#endif

extern "C" int execute_internal_call(void *cpu_state, int call);

// HACK: for Debugging
static x86_cpu_state *__current_state;

static std::mutex segv_lock;
/*
 * The segfault handler.
 */
static void segv_handler([[maybe_unused]] int signo,
                         [[maybe_unused]] siginfo_t *info,
                         [[maybe_unused]] void *context) {
    segv_lock.lock();
#if defined(ARCH_X86_64)
    unsigned long rip = ((ucontext_t *)context)->uc_mcontext.gregs[REG_RIP];
#else
    unsigned long rip = 0;
#endif

    util::global_logger.fatal(
        "SEGMENTATION FAULT: code={:#x}, rip={:#x}, virtual-address={}\n",
        info->si_code, rip, info->si_addr);

    unsigned i = 0;
    auto range = ctx_->get_thread_range();
    for (auto it = range.first; it != range.second; it++) {
        auto state = (x86_cpu_state *)it->second->get_cpu_state();
        util::global_logger.info("Thread[{}] Guest PC: {:#x}\n", i,
                                 util::copy(state->PC));
        util::global_logger.info("Thread[{}] FS: {:#x}\n", i,
                                 util::copy(state->FS));
        i++;
    }

    segv_lock.unlock();
    exit(1);
}

/*
 * Initialises signal handling
 */
static void init_signals() {
    struct sigaction sa = {0};

    // Capture SIGSEGV
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = &segv_handler;
    if (sigaction(SIGSEGV, &sa, nullptr) < 0) {
        throw std::runtime_error("unable to initialise signal handling");
    }
}

static const void *find_host_stack_bottom(int argc, const char **argv,
                                          size_t &argv_size, size_t &envp_size,
                                          size_t &auxv_size,
                                          size_t &asciiz_size) {
    // structure of stack:
    // argc
    // argv[0]
    // ...
    // argv[argc] = nullptr
    // envp[0]
    // ...
    // envp[term] = nullptr
    // auxv[0]
    // ...
    // auxv[term] = AT_NULL
    // padding (unspecified size) -> use argc[0] to find the bottom of the stack
    // argument ASCIIZ strings
    // enviroment ASCIIZ string
    // end marker = nullptr

    const char **envp = argv + argc + 1;
    argv_size = (argc + 1) * sizeof(char *);

    do {
        envp_size += sizeof(char *);
    } while (*(envp++));

    const Elf64_auxv_t *auxv = reinterpret_cast<const Elf64_auxv_t *>(envp);
    do {
        auxv_size += sizeof(Elf64_auxv_t);
    } while ((auxv++)->a_type != AT_NULL);

    uint64_t padding_end = (uint64_t)(argv[0]) & ~0xFul;

    // both argument and environment strings are now at the end
    const uint64_t *asciiz_end =
        reinterpret_cast<const uint64_t *>(padding_end);
    do {
        asciiz_end++;
    } while (*asciiz_end);

    asciiz_size = (const char *)asciiz_end - (const char *)auxv;

    return asciiz_end;
}

void push_asciiz_strings(const void *&host_bottom, void *&guest_bottom,
                         size_t asciiz_size) {
    (intptr_t &)host_bottom -= asciiz_size;
    (intptr_t &)guest_bottom -= asciiz_size;
    memcpy(guest_bottom, host_bottom, asciiz_size);
}

void push_aux_vectors(const void *&host_bottom, void *&guest_bottom,
                      size_t auxv_size, intptr_t entry_point) {
    (intptr_t &)host_bottom -= auxv_size;
    (intptr_t &)guest_bottom -= auxv_size;
    memcpy(guest_bottom, host_bottom, auxv_size);

    Elf64_Ehdr *ehdr = reinterpret_cast<Elf64_Ehdr *>(&guest_exec_base);

    Elf64_auxv_t *auxv = reinterpret_cast<Elf64_auxv_t *>(guest_bottom);

    const auto set_auxv = [auxv](int type, uint64_t value) {
        for (auto av = auxv; av->a_type != AT_NULL; av++) {
            if (av->a_type == type) {
                av->a_un.a_val = value;
                return;
            }
        }
    };

    set_auxv(AT_PHDR, (reinterpret_cast<uint64_t>(&guest_exec_base) +
                       sizeof(Elf64_Ehdr)));
    set_auxv(AT_PHENT, ehdr->e_phentsize);
    set_auxv(AT_PHNUM, ehdr->e_phnum);
    set_auxv(AT_BASE, reinterpret_cast<uint64_t>(&guest_exec_base));
    set_auxv(AT_ENTRY, entry_point);
    set_auxv(AT_HWCAP, 0);
    set_auxv(AT_HWCAP2, 0);
    // TODO: maybe more aux vector entries are needed
}

const void *rebase_stack_pointer(const void *sp, const void *host_bottom,
                                 const void *guest_bottom) {
    const void *result = (const void *)((uintptr_t)sp - (uintptr_t)host_bottom +
                                        (uintptr_t)guest_bottom);
    return result;
}

void push_env_strings(const void *&host_bottom, void *&guest_bottom,
                      size_t envp_size) {
    (intptr_t &)host_bottom -= envp_size;
    (intptr_t &)guest_bottom -= envp_size;
    memcpy(guest_bottom, host_bottom, envp_size);
    for (const char **p = (const char **)guest_bottom; *p; p++) {
        *p = (const char *)rebase_stack_pointer(*p, host_bottom, guest_bottom);
    }
}

void push_arg_strings(const void *&host_bottom, void *&guest_bottom,
                      size_t argv_size) {
    (intptr_t &)host_bottom -= argv_size;
    (intptr_t &)guest_bottom -= argv_size;
    memcpy(guest_bottom, host_bottom, argv_size);
    for (const char **p = (const char **)guest_bottom; *p; p++) {
        *p = (const char *)rebase_stack_pointer(*p, host_bottom, guest_bottom);
    }
}

static void *setup_guest_stack(int argc, const char **argv, void *stack_bottom,
                               intptr_t entry_point) {

    size_t argv_size = 0, envp_size = 0, auxv_size = 0, asciiz_size = 0;
    const void *host_bottom = find_host_stack_bottom(
        argc, argv, argv_size, envp_size, auxv_size, asciiz_size);

    push_asciiz_strings(host_bottom, stack_bottom, asciiz_size);
    push_aux_vectors(host_bottom, stack_bottom, auxv_size, entry_point);
    push_env_strings(host_bottom, stack_bottom, envp_size);
    push_arg_strings(host_bottom, stack_bottom, argv_size);

    // Push argc
    (intptr_t &)stack_bottom -= sizeof(int64_t);
    *reinterpret_cast<int64_t *>(stack_bottom) = argc;
    return stack_bottom;
}

static std::unordered_map<unsigned long, void *> fn_addrs;

/*
 * Initialises the dynamic runtime for the guest program that is about to be
 * executed.
 */
extern "C" void *initialise_dynamic_runtime(unsigned long entry_point, int argc,
                                            const char **argv) {
    const char *flag = getenv("ARANCINI_ENABLE_LOG");
    if (flag) {
        if (util::case_ignore_string_equal(flag, "true"))
            util::global_logger.enable(true);
        else if (util::case_ignore_string_equal(flag, "false"))
            util::global_logger.enable(false);
        else
            throw std::runtime_error(
                "ARANCINI_ENABLE_LOG must be set to either true or false");
    }

    // Determine logger level
    flag = getenv("ARANCINI_LOG_LEVEL");
    if (flag && util::global_logger.is_enabled()) {
        if (util::case_ignore_string_equal(flag, "debug"))
            util::global_logger.set_level(util::global_logging::levels::debug);
        else if (util::case_ignore_string_equal(flag, "info"))
            util::global_logger.set_level(util::global_logging::levels::info);
        else if (util::case_ignore_string_equal(flag, "warn"))
            util::global_logger.set_level(util::global_logging::levels::warn);
        else if (util::case_ignore_string_equal(flag, "error"))
            util::global_logger.set_level(util::global_logging::levels::error);
        else if (util::case_ignore_string_equal(flag, "fatal"))
            util::global_logger.set_level(util::global_logging::levels::fatal);
        else
            throw std::runtime_error(
                "ARANCINI_LOG_LEVEL must be set to one among: debug, info, "
                "warn, error or fatal (case-insensitive)");
    } else if (util::global_logger.is_enabled()) {
        std::cerr << "Logger enabled without explicit log level; setting log "
                     "level to default [info]\n";
        util::global_logger.set_level(util::global_logging::levels::info);
    }

    // Determine logger level
    flag = getenv("ARANCINI_LOG_STREAM");
    if (flag && util::global_logger.is_enabled()) {
        if (util::case_ignore_string_equal(flag, "stdout"))
            util::global_logger.set_output_file(stdout);
        else if (util::case_ignore_string_equal(flag, "stderr"))
            util::global_logger.set_output_file(stderr);
        else {
            // Open file
            FILE *out = std::fopen(flag, "w");
            if (!out)
                throw std::runtime_error("Unable to open requested file for "
                                         "the Arancini logger stream");

            util::global_logger.set_output_file(out);
        }
    } else if (util::global_logger.is_enabled()) {
        std::cerr << "Logger enabled without explicit log stream; the default "
                     "log stream will be used [stderr]\n";
    }

    util::global_logger.info("arancini: dbt: initialise\n");

    bool optimise = true;

    flag = getenv("ARANCINI_OPTIMIZE_FLAGS");

    if (flag) {
        if (util::case_ignore_string_equal(flag, "true"))
            optimise = true;
        else if (util::case_ignore_string_equal(flag, "false"))
            optimise = false;
    }

    // Capture interesting signals, such as SIGSEGV.
    init_signals();

    // Create an execution context for the given input (guest) and output
    // (host) architecture.
    ctx_ = new execution_context(ia, oe, optimise);

    // Create a memory area for the stack.
    // FIXME hardcoded stack_size and memory size
    unsigned long stack_size = 0x10000;
    void *stack_base =
        ctx_->add_memory_region(0x10000000 - stack_size, stack_size, true);

    // Create the main execution thread.
    auto main_thread = ctx_->create_execution_thread();

    // Initialise the CPU state structure with the PC set to the entry point
    // of the guest program, and an emulated stack pointer at the top of the
    // emulated address space.
    x86_cpu_state *x86_state = (x86_cpu_state *)main_thread->get_cpu_state();
    __current_state = x86_state;
    x86_state->PC = entry_point;

    x86_state->RSP = reinterpret_cast<uint64_t>(setup_guest_stack(
        argc, argv, reinterpret_cast<char *>(stack_base) + stack_size,
        entry_point));
    // x86_state->GS = (unsigned long long)ctx_->get_memory_ptr(0);
    x86_state->X87_STACK_BASE =
        (intptr_t)mmap(NULL, 64, PROT_READ | PROT_WRITE,
                       MAP_ANONYMOUS | MAP_PRIVATE, -1, 0) -
        (intptr_t)ctx_->get_memory_ptr(0);
    x86_state->X87_TAG = std::uint16_t(0xFFFF);
    x86_state->X87_CTRL = std::uint16_t(0x037F);

    // Report on various information for useful debugging purposes.
    util::global_logger.info("state={} pc={:#x} stack={:#x}\n",
                             fmt::ptr(x86_state), util::copy(x86_state->PC),
                             util::copy(x86_state->RSP));

    // Initialisation of the runtime is complete - return a pointer to the
    // raw CPU state structure so that the static code can use it for
    // emulation.
    return main_thread->get_cpu_state();
}

/**
 * Register a static function so it can be called from the main loop
 */
extern "C" void register_static_fn_addr(unsigned long guest_addr,
                                        void *fn_addr) {
    fn_addrs.emplace(guest_addr, fn_addr);
}

/**
 * Look up a static function. Returns nullptr if the function was not found.
 */
extern "C" void *lookup_static_fn_addr(unsigned long guest_addr) {
    if (auto item = fn_addrs.find(guest_addr); item != fn_addrs.end()) {
        return item->second;
    }

    return nullptr;
}

/*
 * Entry point from /static/ code when the CPU jumps to an address that
 * hasn't been translated.
 */
extern "C" int invoke_code(void *cpu_state) { return ctx_->invoke(cpu_state); }

/*
 * Entry point from /static/ code when internal call needs to be executed.
 */
extern "C" int execute_internal_call(void *cpu_state, int call) {
    return ctx_->internal_call(cpu_state, call);
}

extern "C" void poison(char *s) {
    std::cerr << "Unimplemened Instr: " << s << "\n";
    abort();
}

extern "C" void finalize() {
    delete ctx_;
    exit(0);
}

extern "C" void clk(void *cpu_state, char *s) {
    ctx_->get_thread(cpu_state)->clk(s);
}

extern "C" void alert() { std::cout << "Top of MainLoop!\n"; }
