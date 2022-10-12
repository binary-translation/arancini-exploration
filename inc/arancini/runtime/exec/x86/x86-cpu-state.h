#pragma once

namespace arancini::runtime::exec::x86 {
struct xmmreg {
	unsigned long l, h;
};

struct x86_cpu_state {
	/* 0 */ unsigned long pc;
	/* 1 */ unsigned long rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi;
	/* 9 */ unsigned long r8, r9, r10, r11, r12, r13, r14, r15;
	/* 17 */ unsigned char zf, cf, of, sf, pf;
	/* 22 */ xmmreg xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15;
	/* 38 */ unsigned long fs, gs;
};
} // namespace arancini::runtime::exec::x86
