.text
.align 16
.type call_native, %function
.globl call_native
call_native:
    // Push to stack callee-saved registers two at a time on the stack
    // Adjust stack pointer by 16 (for 2x8 registers)
    stp x29, x30, [sp, #-16]!
    stp x28, x27, [sp, #-16]!
    stp x26, x25, [sp, #-16]!
    stp x24, x23, [sp, #-16]!
    stp x22, x21, [sp, #-16]!
    stp x20, x19, [sp, #-16]!

    // Registers x9-x17 are temporary; can be clobbered

    // Register x18 disable for use in register allocator
    // Untouched here

    // Register x0-x7 may also be clobbered

    mov x29, x1 // Set frame pointer to x86 context block
    mov x28, x2 // Set memory base register

    blr x0 // call translation block

    // Restore previously stored callee-saved register
    ldp x19, x20, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x27, x28, [sp], #16
    ldp x29, x30, [sp], #16

    // NOTE: x0 not restored on stack, set with return code by backend

    ret // Return to runtime
.size call_native,.-call_native
