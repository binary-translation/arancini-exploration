#include <stdint.h>

int64_t test64_1(void)
{
    int64_t op = 42, rax_val = 10;
    register int64_t *rax asm("rax") = &rax_val;
    register int64_t *rsi asm("rsi") = &op;

    asm volatile ("imul %1"
    //               : "+r"(rax)
    //               : );
                  : "+r"(rax)
                  : "r"(rsi));

    // int64_t rdx_val = 42;
    // register int64_t *rdx asm("rdx") = &rdx_val;

    return op;
}

int8_t test8_1(void)
{
    int8_t op = 42, al_val = 10;
    int16_t ax_val;
    register int16_t *ax asm("ax") = &ax_val;
    register int8_t *sil asm("sil") = &op;

    asm volatile ("imul %1"
                   : "+r"(ax)
                   : "r"(sil));

    return op;
}
