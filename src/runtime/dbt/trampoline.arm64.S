.text
.align 16
.type call_native, %function
.globl call_native
call_native:
    stp x29, x30, [sp, #-16]! // Save x29 and x30 on the stack and adjust sp by -16 bytes

    stp x19, x20, [sp, #-16]! // Save x19 and x20 on the stack and adjust sp by -16 bytes
    stp x21, x22, [sp, #-16]! // Save x21 and x22 on the stack and adjust sp by -16 bytes
    stp x23, x24, [sp, #-16]! // Save x23 and x24 on the stack and adjust sp by -16 bytes
    stp x25, x26, [sp, #-16]! // Save x25 and x26 on the stack and adjust sp by -16 bytes
    stp x27, x28, [sp, #-16]! // Save x27 and x28 on the stack and adjust sp by -16 bytes

    str x0, x1, [sp, #-16]! // Save x0 and x1 on the stack and adjust sp by -16 bytes
    stp x2, x3, [sp, #-16]! // Save x2 and x3 on the stack and adjust sp by -16 bytes
    stp x4, x5, [sp, #-16]! // Save x4 and x5 on the stack and adjust sp by -16 bytes
    stp x6, x7, [sp, #-16]! // Save x6 and x7 on the stack and adjust sp by -16 bytes
    stp x8, x9, [sp, #-16]! // Save x8 and x9 on the stack and adjust sp by -16 bytes
    stp x10, x11, [sp, #-16]! // Save x10 and x11 on the stack and adjust sp by -16 bytes
    stp x12, x13, [sp, #-16]! // Save x12 and x13 on the stack and adjust sp by -16 bytes
    stp x14, x15, [sp, #-16]! // Save x14 and x15 on the stack and adjust sp by -16 bytes
    stp x16, x17, [sp, #-16]! // Save x16 and x17 on the stack and adjust sp by -16 bytes
    stp x18, x19, [sp, #-16]! // Save x18 and x19 on the stack and adjust sp by -16 bytes


    // Set frame pointer to x86 context block
    mov x29, x1

    blr x0 // call translation block

    ldp x18, x19, [sp], #16 // Restore x18 and x19 from the stack and adjust sp by 16 bytes
    ldp x16, x17, [sp], #16 // Restore x16 and x17 from the stack and adjust sp by 16 bytes
    ldp x14, x15, [sp], #16 // Restore x14 and x15 from the stack and adjust sp by 16 bytes
    ldp x12, x13, [sp], #16 // Restore x12 and x13 from the stack and adjust sp by 16 bytes
    ldp x10, x11, [sp], #16 // Restore x10 and x11 from the stack and adjust sp by 16 bytes
    ldp x8, x9, [sp], #16 // Restore x8 and x9 from the stack and adjust sp by 16 bytes
    ldp x6, x7, [sp], #16 // Restore x6 and x7 from the stack and adjust sp by 16 bytes
    ldp x4, x5, [sp], #16 // Restore x4 and x5 from the stack and adjust sp by 16 bytes


    // NOTE: x0 not restored on stack, used for return code
    ldp x2, x3, [sp], #32 // Restore x2 and x3 from the stack and adjust sp by 32 bytes
    ldr x1, [sp, #-8]     // Restore only x1

    ldp x27, x28, [sp], #16 // Restore x27 and x28 from the stack and adjust sp by 16 bytes
    ldp x25, x26, [sp], #16 // Restore x25 and x26 from the stack and adjust sp by 16 bytes
    ldp x23, x24, [sp], #16 // Restore x23 and x24 from the stack and adjust sp by 16 bytes
    ldp x21, x22, [sp], #16 // Restore x21 and x22 from the stack and adjust sp by 16 bytes
    ldp x19, x20, [sp], #16 // Restore x19 and x20 from the stack and adjust sp by 16 bytes

    ldp x29, x30, [sp], #16 // Restore x29 and x30 from the stack and adjust sp by 16 bytes

    ret // Return to runtime
.size call_native,.-call_native
