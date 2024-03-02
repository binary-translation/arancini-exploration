#include <stddef.h>
#include <stdint.h>

typedef struct lib_info {
	unsigned char *base;
	struct lib_info *next;
} lib_info;

extern lib_info *lib_info_list;
extern lib_info *lib_info_list_tail;
extern int lib_count;

extern unsigned char guest_base;

static lib_info local_libinfo = { &guest_base, NULL };


static __attribute__((constructor)) void init_lib()
{
	if (!lib_info_list_tail) {
		// First element. Set as head and tail
		lib_info_list = lib_info_list_tail = &local_libinfo;
	} else {
		// Link after last element and move tail.
		lib_info_list_tail->next = &local_libinfo;
		lib_info_list_tail = &local_libinfo;
	}

	lib_count++;
}
