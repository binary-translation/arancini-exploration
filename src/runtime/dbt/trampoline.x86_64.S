.text
.align 16
.type call_native, %function
.globl call_native
call_native:
    push %rbp
    mov %rsi, %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15

    // Code Ptr in RDI, CPU state in RBP
    call *%rdi

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp

    ret
.size call_native,.-call_native
