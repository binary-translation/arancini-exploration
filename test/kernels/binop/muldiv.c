#include <stdint.h>

int64_t test(void)
{
    int64_t op = 42, rax_val = 10;
    register int64_t *rax asm("rax") = &rax_val;

    asm volatile ("imul %1"
    //               : "+r"(rax)
    //               : );
                  : "+r"(rax)
                  : "r"(op));

    // int64_t rdx_val = 42;
    // register int64_t *rdx asm("rdx") = &rdx_val;

    return op;
}