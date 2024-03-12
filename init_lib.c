#include <stddef.h>
#include <stdint.h>

#ifndef TLS_LEN
#define TLS_LEN 0
#endif
#ifndef TLS_SIZE
#define TLS_SIZE 0
#endif
#ifndef TLS_ALIGN
#define TLS_ALIGN 0
#endif

typedef struct lib_info {
	unsigned char *base;
	size_t *dynv;
	void *tls_image;
	size_t tls_len;
	size_t tls_size;
	size_t tls_align;
	uint64_t tls_offset;
	struct lib_info *next;
} lib_info;

extern lib_info *lib_info_list;
extern lib_info *lib_info_list_tail;
extern int lib_count;

extern uint64_t tls_offset __attribute__((weak));

typedef struct {
	uint64_t *location;
	uint64_t addend;
} tp_reloc;
extern tp_reloc __TPREL_INIT[];

extern size_t __guest___DYNAMIC;
extern unsigned char guest_base;
extern unsigned char guest_tls;

static lib_info local_libinfo = { &guest_base, &__guest___DYNAMIC, &guest_tls, TLS_LEN, TLS_SIZE, TLS_ALIGN, 0, NULL };


static __attribute__((constructor)) void init_lib()
{
#if TLS_SIZE > 0
		tls_offset += TLS_SIZE + TLS_ALIGN - 1;
		tls_offset -= (tls_offset + (uintptr_t)&guest_tls) & (TLS_ALIGN - 1);
#endif
	uint64_t tls_off = tls_offset;

	local_libinfo.tls_offset = tls_off;

	if (!lib_info_list_tail) {
		// First element. Set as head and tail
		lib_info_list = lib_info_list_tail = &local_libinfo;
	} else {
		// Link after last element and move tail.
		lib_info_list_tail->next = &local_libinfo;
		lib_info_list_tail = &local_libinfo;
	}

	lib_count++;

	const tp_reloc *tprel = __TPREL_INIT;
	while (tprel->location) {
		*(tprel->location) = tls_off + tprel->addend;
		tprel++;
	}
}
