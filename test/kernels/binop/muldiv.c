#include <stdint.h>

int64_t imul_64_1(void)
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

int8_t imul_8_1(void)
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

uint64_t mul_64(void)
{
    uint64_t op = 42, rax_val = 55;
    register uint64_t *ax asm("rax") = &rax_val;
    register uint64_t *rsi asm("rsi") = &op;

    asm volatile ("mul %0"
                  : 
                  : "r"(rsi));

    return rax_val;
}

uint64_t idiv_64(void)
{
    uint64_t op = 42, rax_val = 12345;
    register uint64_t *ax asm("rax") = &rax_val;
    register uint64_t *rsi asm("rsi") = &op;

    asm volatile ("idiv %0"
                  :
                  : "r"(rsi));

    return rax_val;
}