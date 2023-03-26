//
// Created by simon on 26.03.23.
//

#pragma once

template <typename... Args> static inline uint64_t native_syscall(uint64_t syscall_no, Args... arg1);
#if defined(ARCH_RISCV64)
#include <linux/unistd.h>
template <> inline uint64_t native_syscall(uint64_t syscall_no)
{
	register uint64_t a0 __asm__("a0");
	register uint64_t a7 __asm__("a7") = syscall_no;
	__asm__ __volatile__("ecall\n\t" : "=r"(a0) : "r"(a7) : "memory");
	return a0;
}
template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1)
{
	register uint64_t a0 __asm__("a0") = arg1;
	register uint64_t a7 __asm__("a7") = syscall_no;
	__asm__ __volatile__("ecall\n\t" : "=r"(a0) : "r"(a7), "0"(a0) : "memory");
	return a0;
}
template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2)
{
	register uint64_t a0 __asm__("a0") = arg1;
	register uint64_t a1 __asm__("a1") = arg2;
	register uint64_t a7 __asm__("a7") = syscall_no;
	__asm__ __volatile__("ecall\n\t" : "=r"(a0) : "r"(a7), "0"(a0), "r"(a1) : "memory");
	return a0;
}
template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
	register uint64_t a0 __asm__("a0") = arg1;
	register uint64_t a1 __asm__("a1") = arg2;
	register uint64_t a2 __asm__("a2") = arg3;
	register uint64_t a7 __asm__("a7") = syscall_no;
	__asm__ __volatile__("ecall\n\t" : "=r"(a0) : "r"(a7), "0"(a0), "r"(a1), "r"(a2) : "memory");
	return a0;
}
template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
	register uint64_t a0 __asm__("a0") = arg1;
	register uint64_t a1 __asm__("a1") = arg2;
	register uint64_t a2 __asm__("a2") = arg3;
	register uint64_t a3 __asm__("a3") = arg4;
	register uint64_t a7 __asm__("a7") = syscall_no;
	__asm__ __volatile__("ecall\n\t" : "=r"(a0) : "r"(a7), "0"(a0), "r"(a1), "r"(a2), "r"(a3) : "memory");
	return a0;
}
template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
	register uint64_t a0 __asm__("a0") = arg1;
	register uint64_t a1 __asm__("a1") = arg2;
	register uint64_t a2 __asm__("a2") = arg3;
	register uint64_t a3 __asm__("a3") = arg4;
	register uint64_t a4 __asm__("a4") = arg5;
	register uint64_t a7 __asm__("a7") = syscall_no;
	__asm__ __volatile__("ecall\n\t" : "=r"(a0) : "r"(a7), "0"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4) : "memory");
	return a0;
}
template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
	register uint64_t a0 __asm__("a0") = arg1;
	register uint64_t a1 __asm__("a1") = arg2;
	register uint64_t a2 __asm__("a2") = arg3;
	register uint64_t a3 __asm__("a3") = arg4;
	register uint64_t a4 __asm__("a4") = arg5;
	register uint64_t a5 __asm__("a5") = arg6;
	register uint64_t a7 __asm__("a7") = syscall_no;
	__asm__ __volatile__("ecall\n\t" : "=r"(a0) : "r"(a7), "0"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5) : "memory");
	return a0;
}
#endif