.text
.align 16 // TODO: determine right alignment
.type call_native, %function
.globl call_native
call_native:
    // Push callee-saved registers to stack
    // TODO: handle float regs
    addi sp, sp, -104  // Allocate space on the stack for 14 registers
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
    sd s10,88(sp)
    sd s11,96(sp)
    sd gp, 104(sp)

    // Get state pointer into right register
    mv fp, a1

    // Get mem base into register
    mv t6, a2

    // Load x86 registers into RISCV ones

    ld x16, 8(fp)
    ld x17, 16(fp)
    ld x18, 24(fp)
    ld x19, 32(fp)
    ld x20, 40(fp)
    ld x21, 48(fp)
    ld x22, 56(fp)
    ld x23, 64(fp)


    // Call function
    jalr a0


    // Save loaded x86 registers

    sd x23, 64(fp)
    sd x22, 56(fp)
    sd x21, 48(fp)
    sd x20, 40(fp)
    sd x19, 32(fp)
    sd x18, 24(fp)
    sd x17, 16(fp)
    sd x16, 8(fp)


    // Restore callee-saved registers from stack
    // TODO: handle float regs
    ld gp,104(sp)      // Restore the callee-saved registers
    ld s11,96(sp)
    ld s10,88(sp)
    ld s9, 80(sp)
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
    addi sp, sp, 104   // Deallocate the stack space

    # Set return value to 0 (the error code)
    # li a0, 0

    ret
.size call_native,.-call_native

