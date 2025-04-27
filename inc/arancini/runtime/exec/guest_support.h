#pragma once

#define GUEST_DECL(ty, x) ty __guest__##x
#define GUEST(x) __guest__##x

#define LIBC_SUPPORT MUSL

#if LIBC_SUPPORT == MUSL

#define Phdr void
#define Sym void
#define Elf_Symndx void

extern "C" {
struct guest_pthread {
    /* Part 1 -- these fields may be external or
     * internal (accessed via asm) ABI. Do not change. */
    guest_pthread *self;
    uintptr_t *dtv;
    guest_pthread *prev, *next; /* non-ABI */
    uintptr_t sysinfo;
    uintptr_t canary;

    /* Part 2 -- implementation details, non-ABI. */
    int tid;
    int errno_val;
    volatile int detach_state;
    volatile int cancel;
    volatile unsigned char canceldisable, cancelasync;
    unsigned char tsd_used : 1;
    unsigned char dlerror_flag : 1;
    unsigned char *map_base;
    size_t map_size;
    void *stack;
    size_t stack_size;
    size_t guard_size;
    void *result;
    struct __ptcb *cancelbuf;
    void **tsd;
    struct {
        volatile void *volatile head;
        long off;
        volatile void *volatile pending;
    } robust_list;
    int h_errno_val;
    volatile int timer_id;
    locale_t locale;
    volatile int killlock[1];
    char *dlerror_buf;
    void *stdio_locks;
};

enum {
    DT_EXITED = 0,
    DT_EXITING,
    DT_JOINABLE,
    DT_DETACHED,
};

typedef guest_pthread *guest_pthread_t;

struct tls_module {
    struct tls_module *next;
    void *image;
    size_t len, size, align, offset;
};

struct dso {
#if DL_FDPIC
    struct fdpic_loadmap *loadmap;
#else
    unsigned char *base;
#endif
    char *name;
    size_t *dynv;
    struct dso *next, *prev;

    Phdr *phdr;
    int phnum;
    size_t phentsize;
    Sym *syms;
    Elf_Symndx *hashtab;
    uint32_t *ghashtab;
    int16_t *versym;
    char *strings;
    struct dso *syms_next, *lazy_next;
    size_t *lazy, lazy_cnt;
    unsigned char *map;
    size_t map_len;
    dev_t dev;
    ino_t ino;
    char relocated;
    char constructed;
    char kernel_mapped;
    char mark;
    char bfs_built;
    char runtime_loaded;
    struct dso **deps, *needed_by;
    size_t ndeps_direct;
    size_t next_dep;
    guest_pthread_t ctor_visitor;
    char *rpath_orig, *rpath;
    struct tls_module tls;
    size_t tls_id;
    size_t relro_start, relro_end;
    uintptr_t *new_dtv;
    unsigned char *new_tls;
    struct td_index *td_index;
    struct dso *fini_next;
    char *shortname;
#if DL_FDPIC
    unsigned char *base;
#else
    struct fdpic_loadmap *loadmap;
#endif
    struct funcdesc {
        void *addr;
        size_t *got;
    } *funcdescs;
    size_t *got;
    char buf[];
};

struct libc {
    char can_do_threads;
    char threaded;
    char secure;
    volatile signed char need_locks;
    int threads_minus_1;
    size_t *auxv;
    struct tls_module *tls_head;
    size_t tls_size, tls_align, tls_cnt;
    size_t page_size;
    struct __locale_struct global_locale;
};

// Weak, because they are declared in the libc library and might not be there,
// if we don't have a musl libc.
GUEST_DECL(extern void *, __environ) __attribute__((weak));
GUEST_DECL(extern struct dso **, main_ctor_queue) __attribute__((weak));
GUEST_DECL(extern int, __malloc_replaced) __attribute__((weak));
GUEST_DECL(extern struct libc, __libc) __attribute__((weak));
GUEST_DECL(extern volatile int, __thread_list_lock) __attribute__((weak));
GUEST_DECL(extern volatile int, __sysinfo) __attribute__((weak));

// Weak because they are declared in the executable, will always be there, so
// never point to NULL.
extern size_t guest_exec_DYNAMIC __attribute__((weak));
extern unsigned char guest_exec_base __attribute__((weak));
extern tls_module guest_exec_tls __attribute__((weak));
extern size_t tls_offset __attribute__((weak));
}

#else
#error None set
#endif

struct lib_info {
    unsigned char *base;
    size_t *dynv;
    void *tls_image;
    size_t tls_len;
    size_t tls_size;
    size_t tls_align;
    uint64_t tls_offset;
    uint64_t **dtp_mod;
    uint64_t *func_map;
    struct lib_info *next;
};