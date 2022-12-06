.text
.align 16
.type call_native, %function
.globl call_native
call_native:
    // Push callee-saved registers to the stack
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15

    // Remember the stack pointer, so we can do what we want
    // with the stack elsewhere.
    mov %rsp, %r15

    // Move the CPU state pointer to RBP for convenience.
    mov %rsi, %rbp

    // At this point, the pointer to the native code block is
    // in RDI, R15 holds the remembered stack pointer, and RBP
    // has the CPU state pointer.
    call *%rdi

    // Restore the stack pointer, undoing anything we did further down the call chain.
    mov %r15, %rsp

    // Restore callee-saved register.
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp

    // Return.  EAX should have been set by native code.
    ret
.size call_native,.-call_native
