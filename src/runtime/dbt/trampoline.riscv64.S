.text
.align 16 // TODO: determine right alignment
.type call_native, %function
.globl call_native
call_native:
    // Push callee-saved registers to stack
    // TODO: handle float regs
    addi sp, sp, -80  // Allocate space on the stack for 20 registers
    sd ra, 0(sp)      // Save the return address
    sd s0, 8(sp)      // Save the callee-saved registers
    sd s1, 16(sp)
    sd s2, 24(sp)
    sd s3, 32(sp)
    sd s4, 40(sp)
    sd s5, 48(sp)
    sd s6, 56(sp)
    sd s7, 64(sp)
    sd s8, 72(sp)
    sd s9, 80(sp)

    // Get state pointer into right register
    mv fp, a1

    // Call function
    jalr a0

    // Restore callee-saved registers from stack
    // TODO: handle float regs
    ld s9, 80(sp)      // Restore the callee-saved registers
    ld s8, 72(sp)
    ld s7, 64(sp)
    ld s6, 56(sp)
    ld s5, 48(sp)
    ld s4, 40(sp)
    ld s3, 32(sp)
    ld s2, 24(sp)
    ld s1, 16(sp)
    ld s0, 8(sp)

    ld ra, 0(sp)      // Restore the return address
    addi sp, sp, 80   // Deallocate the stack space

    # Set return value to 0 (the error code)
    li a0, 0

    ret
.size call_native,.-call_native

