//
// Created by simon on 26.03.23.
//

#pragma once

#include <cstdint>
#include <linux/unistd.h>

template <typename... Args> static inline uint64_t native_syscall(uint64_t syscall_no, Args... arg1);

#if defined(ARCH_RISCV64)
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
#endif // ARCH_RISCV64

template <typename... Args> static inline uint64_t native_syscall(uint64_t syscall_no, Args... arg1);
#if defined(ARCH_AARCH64)
#include <linux/unistd.h>
template <> inline uint64_t native_syscall(uint64_t syscall_no)
{
	register uint64_t x0 __asm__("x0");
	register uint64_t x8 __asm__("x8") = syscall_no;
	__asm__ __volatile__("svc #0\n\t" : "=r"(x0) : "r"(x8) : "memory");
	return x0;
}
template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1)
{
	register uint64_t x0 __asm__("x0") = arg1;
	register uint64_t x8 __asm__("x8") = syscall_no;
	__asm__ __volatile__("svc #0\n\t" : "=r"(x0) : "r"(x8), "0"(x0) : "memory");
	return x0;
}
template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2)
{
	register uint64_t x0 __asm__("x0") = arg1;
	register uint64_t x1 __asm__("x1") = arg2;
	register uint64_t x8 __asm__("x8") = syscall_no;
	__asm__ __volatile__("svc #0\n\t"
                         : "=r"(x0)
                         : "r"(x8), "0"(x0), "r"(x1)
                         : "memory");
	return x0;
}
template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
	register uint64_t x0 __asm__("x0") = arg1;
	register uint64_t x1 __asm__("x1") = arg2;
	register uint64_t x2 __asm__("x2") = arg3;
	register uint64_t x8 __asm__("x8") = syscall_no;
	__asm__ __volatile__("svc #0\n\t"
                         : "=r"(x0)
                         : "r"(x8), "0"(x0), "r"(x1), "r"(x2)
                         : "memory");
	return x0;
}
template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
	register uint64_t x0 __asm__("x0") = arg1;
	register uint64_t x1 __asm__("x1") = arg2;
	register uint64_t x2 __asm__("x2") = arg3;
	register uint64_t x3 __asm__("x3") = arg4;
	register uint64_t x8 __asm__("x8") = syscall_no;
	__asm__ __volatile__("svc #0\n\t"
                         : "=r"(x0)
                         : "r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3)
                         : "memory");
	return x0;
}
template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
	register uint64_t x0 __asm__("x0") = arg1;
	register uint64_t x1 __asm__("x1") = arg2;
	register uint64_t x2 __asm__("x2") = arg3;
	register uint64_t x3 __asm__("x3") = arg4;
	register uint64_t x4 __asm__("x4") = arg5;
	register uint64_t x8 __asm__("x8") = syscall_no;
	__asm__ __volatile__("svc #0\n\t"
                         : "=r"(x0)
                         : "r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4)
                         : "memory");
	return x0;
}
template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
	register uint64_t x0 __asm__("x0") = arg1;
	register uint64_t x1 __asm__("x1") = arg2;
	register uint64_t x2 __asm__("x2") = arg3;
	register uint64_t x3 __asm__("x3") = arg4;
	register uint64_t x4 __asm__("x4") = arg5;
	register uint64_t x5 __asm__("x5") = arg6;
	register uint64_t x8 __asm__("x8") = syscall_no;
	__asm__ __volatile__("svc #0\n\t"
                         : "=r"(x0)
                         : "r"(x8), "0"(x0), "r"(x1), "r"(x2),
                           "r"(x3), "r"(x4), "r"(x5)
                         : "memory");
	return x0;
}
<<<<<<< HEAD
#elif defined(ARCH_X86_64)

template <> inline uint64_t native_syscall(uint64_t syscall_no)
{
	uint64_t retval = syscall_no;
	__asm__ volatile("syscall" : "+a"(retval) : : "memory", "rcx", "r11");
	return retval;
}

template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1)
{
	uint64_t retval = syscall_no;
	__asm__ volatile("syscall" : "+a"(retval) : "D"(arg1) : "memory", "rcx", "r11");
	return retval;
}

template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2)
{
	uint64_t retval = syscall_no;
	__asm__ volatile("syscall" : "+a"(retval) : "D"(arg1), "S"(arg2) : "memory", "rcx", "r11");
	return retval;
}

template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
	uint64_t retval = syscall_no;
	__asm__ volatile("syscall" : "+a"(retval) : "D"(arg1), "S"(arg2), "d"(arg3) : "memory", "rcx", "r11");
	return retval;
}

template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
	uint64_t retval = syscall_no;
	register uint64_t r10 __asm__("r10") = arg4;
	__asm__ volatile("syscall" : "+a"(retval) : "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10) : "memory", "rcx", "r11");
	return retval;
}

template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
	uint64_t retval = syscall_no;
	register uint64_t r10 __asm__("r10") = arg4;
	register uint64_t r8 __asm__("r8") = arg5;
	__asm__ volatile("syscall" : "+a"(retval) : "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8) : "memory", "rcx", "r11");
	return retval;
}

template <> inline uint64_t native_syscall(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
	uint64_t retval = syscall_no;
	register uint64_t r8 __asm__("r8") = arg5;
	register uint64_t r9 __asm__("r9") = arg6;
	register uint64_t r10 __asm__("r10") = arg4;
	__asm__ volatile("syscall" : "+a"(retval) : "D"(arg1), "S"(arg2), "d"(arg3), "r"(r8), "r"(r9), "r"(r10) : "memory", "rcx", "r11");
	return retval;
}
#endif // ARCH_AARCH64

