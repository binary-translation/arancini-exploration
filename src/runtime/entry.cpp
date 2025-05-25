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

static uint64_t setup_guest_stack(int argc, char **argv, intptr_t stack_top,
                                  execution_context *execution_context,
                                  int start) {
    // Stack pointer always needs to be 16-Byte aligned per ABI convention
    int envc = 0;
    for (; environ[envc]; ++envc)
        ;

    // auxv entries are always 16 Bytes
    stack_top -= ((envc + (argc - (start - 1)) + 1) & 1) * 8;

    // Add auxv to guest stack
    {
        auto *stack =
            (Elf64_auxv_t *)execution_context->get_memory_ptr(stack_top);
        *(--stack) = (Elf64_auxv_t){AT_NULL, {0}};
        //		*(--stack) = (Elf64_auxv_t) {AT_ENTRY, {...}};
        //		*(--stack) = (Elf64_auxv_t) {AT_PHDR, {...r}};
        //		*(--stack) = (Elf64_auxv_t) {AT_PHNUM, {...}};
        //		*(--stack) = (Elf64_auxv_t) {AT_PHENT, {...}};
        *(--stack) = (Elf64_auxv_t){AT_UID, {getauxval(AT_UID)}};
        *(--stack) = (Elf64_auxv_t){AT_GID, {getauxval(AT_GID)}};
        *(--stack) = (Elf64_auxv_t){AT_EGID, {getauxval(AT_EGID)}};
        *(--stack) = (Elf64_auxv_t){AT_EUID, {getauxval(AT_EUID)}};
        *(--stack) = (Elf64_auxv_t){AT_CLKTCK, {getauxval(AT_CLKTCK)}};
        *(--stack) =
            (Elf64_auxv_t){AT_RANDOM,
                           {getauxval(AT_RANDOM) -
                            (uintptr_t)execution_context->get_memory_ptr(
                                0)}}; // TODO Copy/Generate new one?
        *(--stack) = (Elf64_auxv_t){AT_SECURE, {0}};
        *(--stack) = (Elf64_auxv_t){AT_PAGESZ, {getauxval(AT_PAGESZ)}};
        *(--stack) = (Elf64_auxv_t){AT_HWCAP, {0}};
        *(--stack) = (Elf64_auxv_t){AT_HWCAP2, {0}};
        //        *(--stack) = (Elf64_auxv_t) {AT_PLATFORM, {0}};
        *(--stack) =
            (Elf64_auxv_t){AT_EXECFN,
                           {(uintptr_t)argv[0] -
                            (uintptr_t)execution_context->get_memory_ptr(0)}};
        stack_top =
            (intptr_t)(stack) - (intptr_t)execution_context->get_memory_ptr(0);
    }
    // Copy environ to guest stack
    {
        char **stack = (char **)execution_context->get_memory_ptr(stack_top);
        *(--stack) = nullptr;
        // Zero terminated so environ[envc] will be zero and also needs to be
        // copied
        for (int i = envc - 1; i >= 0; i--) {
            *(--stack) =
                (char *)(((uintptr_t)environ[i]) -
                         (uintptr_t)execution_context->get_memory_ptr(0));
        }

        if (&GUEST(__environ) != nullptr) {
            // Exists in guest so set it
            GUEST(__environ) = stack;
        }
        stack_top =
            (intptr_t)stack - (intptr_t)execution_context->get_memory_ptr(0);
    }
    // Copy argv to guest stack
    {
        const char **stack =
            (const char **)execution_context->get_memory_ptr(stack_top);

        // Zero terminated so argv[argc] will be zero and also needs to be
        // copied
        *(--stack) = nullptr;
        for (int i = argc - 1; i >= start; i--) {
            *(--stack) =
                (char *)(((uintptr_t)argv[i]) -
                         (uintptr_t)execution_context->get_memory_ptr(0));
        }
        *(--stack) = (char *)(((uintptr_t)argv[0]) -
                              (uintptr_t)execution_context->get_memory_ptr(0));
        stack_top =
            (intptr_t)stack - (intptr_t)execution_context->get_memory_ptr(0);
    }
    // Copy argc to guest stack
    {
        long *stack = (long *)execution_context->get_memory_ptr(stack_top);
        *(--stack) = argc - (start - 1);
        stack_top =
            (intptr_t)stack - (intptr_t)execution_context->get_memory_ptr(0);
    }

    return (intptr_t)stack_top;
}

extern "C" {
lib_info *lib_info_list = nullptr;
lib_info *lib_info_list_tail = nullptr;
int lib_count = 0;
}

static std::unordered_map<unsigned long, void *> fn_addrs;

/*
 * Initialises the dynamic runtime for the guest program that is about to be
 * executed.
 */
extern "C" void *initialise_dynamic_runtime(unsigned long entry_point, int argc,
                                            char **argv) {
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

    // Create an execution context for the given input (guest) and output (host)
    // architecture.
    ctx_ = new execution_context(ia, oe, optimise);

    // Create a memory area for the stack.
    // FIXME hardcoded stack_size and memory size
    unsigned long stack_size = 0x10000;
    auto stack_base =
        ctx_->add_memory_region(0x10000000 - stack_size, stack_size, true);

    // Create the main execution thread.
    auto main_thread = ctx_->create_execution_thread();

    // Initialise the CPU state structure with the PC set to the entry point of
    // the guest program, and an emulated stack pointer at the top of the
    // emulated address space.
    x86_cpu_state *x86_state = (x86_cpu_state *)main_thread->get_cpu_state();
    __current_state = x86_state;
    x86_state->PC = entry_point;

    x86_state->RSP = setup_guest_stack(
        argc, argv,
        reinterpret_cast<intptr_t>(stack_base) -
            reinterpret_cast<intptr_t>(ctx_->get_memory_ptr(0)) + stack_size,
        ctx_, 1);
    // x86_state->GS = (unsigned long long)ctx_->get_memory_ptr(0);
    x86_state->X87_STACK_BASE =
        (intptr_t)mmap(NULL, 80, PROT_READ | PROT_WRITE,
                       MAP_ANONYMOUS | MAP_PRIVATE, -1, 0) -
        (intptr_t)ctx_->get_memory_ptr(0);
    x86_state->X87_TAG = std::uint16_t(0xFFFF);

    // Report on various information for useful debugging purposes.
    util::global_logger.info("state={} pc={:#x} stack={:#x}\n",
                             fmt::ptr(x86_state), util::copy(x86_state->PC),
                             util::copy(x86_state->RSP));

    if (&GUEST(main_ctor_queue) != nullptr) {
        size_t tls_cnt = 0;
        tls_module *tls_tail = nullptr;

        size_t tls_align = alignof(guest_pthread);

        auto app_dso = new dso();
        app_dso->dynv = &guest_exec_DYNAMIC;
        //		app_dso->base = &guest_exec_base; //Load offset assume 0
        // since not pie
        if (&guest_exec_tls) {
            app_dso->tls_id = tls_cnt = 1;
            app_dso->tls = guest_exec_tls;
            // Correct over alignment
            app_dso->tls.offset -=
                app_dso->tls.align - 1 -
                ((-((uintptr_t)app_dso->tls.image + app_dso->tls.size)) &
                 (app_dso->tls.align - 1));
            GUEST(__libc).tls_head = tls_tail = &app_dso->tls;

#define MAXP2(a, b) (-(-(a) & -(b)))
            tls_align = MAXP2(tls_align, app_dso->tls.align);
        }

        auto dsos = new dso *[lib_count + 2];

        lib_info *lib = lib_info_list;

        for (int i = 0; i < lib_count; ++i, lib = lib->next) {
            auto cur_dso = new dso();
            cur_dso->dynv = lib->dynv;
            cur_dso->base = lib->base;

            for (uint64_t *func_map = lib->func_map; *func_map; func_map += 2) {
                fn_addrs.emplace(func_map[0], (void *)func_map[1]);
            }

            if (lib->tls_len) {

                cur_dso->tls = {nullptr,       lib->tls_image, lib->tls_len,
                                lib->tls_size, lib->tls_align, lib->tls_offset};
                cur_dso->tls_id = tls_cnt++;

                for (uint64_t **dtp_mod = lib->dtp_mod; *dtp_mod; dtp_mod++) {
                    *(*dtp_mod) = tls_cnt;
                }

                tls_align = MAXP2(tls_align, cur_dso->tls.align);
#undef MAXP2
                if (tls_tail) {
                    tls_tail->next = &cur_dso->tls;
                } else {
                    GUEST(__libc).tls_head = &cur_dso->tls;
                }
                tls_tail = &cur_dso->tls;
            }
            dsos[i] = cur_dso;
        }

        dsos[lib_count] = app_dso;
        dsos[lib_count + 1] = nullptr;

        GUEST(main_ctor_queue) = dsos;

        GUEST(__malloc_replaced) = 1; // Prevent guest musl from trying to free
                                      // our allocations using their free

        GUEST(__libc).tls_cnt = tls_cnt;
        GUEST(__libc).tls_align = tls_align;

#define ALIGN(x, y) (((x) + (y) - 1) & -(y))
        GUEST(__libc).tls_size =
            ALIGN((1 + tls_cnt) * sizeof(void *) + tls_offset +
                      sizeof(guest_pthread) + tls_align * 2,
                  tls_align);
#undef ALIGN

        guest_pthread_t td;
        auto *mem = new unsigned char[GUEST(__libc).tls_size]();

        // adapted from __copy_tls in musl
        {
            struct tls_module *p;
            size_t i;
            uintptr_t *dtv;

            dtv = (uintptr_t *)mem;

            mem += GUEST(__libc).tls_size - sizeof(guest_pthread);
            mem -= (uintptr_t)mem & (GUEST(__libc).tls_align - 1);
            td = (guest_pthread_t)mem;

            for (i = 1, p = GUEST(__libc).tls_head; p; i++, p = p->next) {
                dtv[i] = (uintptr_t)(mem - p->offset);
                memcpy(mem - p->offset, p->image, p->len);
            }
            dtv[0] = GUEST(__libc).tls_cnt;
            td->dtv = dtv;
        }

        // adapted from __init_tp in musl
        {
            td->self = td;

            x86_state->FS = reinterpret_cast<uint64_t>(td);

            GUEST(__libc).can_do_threads = 1;

            // Hacky supposedly unstable ABI
            td->detach_state = DT_JOINABLE;

            // Simulate set_tid_address syscall
            x86_state->RAX = 218;
            x86_state->RDI = (uintptr_t)&GUEST(__thread_list_lock);
            execute_internal_call(x86_state, 1);

            td->tid = (int32_t)x86_state->RAX;

            td->locale = &GUEST(__libc).global_locale;
            td->robust_list.head = &td->robust_list.head;
            td->sysinfo = GUEST(__sysinfo);
            td->next = td->prev = td;
        }
    }

    // Initialisation of the runtime is complete - return a pointer to the raw
    // CPU state structure so that the static code can use it for emulation.
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
 * Entry point from /static/ code when the CPU jumps to an address that hasn't
 * been translated.
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
