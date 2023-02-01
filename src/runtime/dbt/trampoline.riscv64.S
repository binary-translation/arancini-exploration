.text
.align 16
.type call_native, %function
.globl call_native
call_native:
    // Push callee-saved registers to stack
    // TODO: handle float regs
    sd sp, 0(sp)
    addi sp, sp, -94
    sd fp, 88(sp)
    sd s1, 80(sp)
    sd s2, 72(sp)
    sd s3, 64(sp)
    sd s4, 56(sp)
    sd s5, 48(sp)
    sd s6, 40(sp)
    sd s7, 32(sp)
    sd s8, 24(sp)
    sd s9, 16(sp)
    sd s10, 8(sp)
    sd s11, 0(sp)

    addi sp, sp, -200
    addi fp, sp, 0

    // Write x86 PC to FP + 0x1234
    sd a1, 0x100(fp)

    // Call function
    jalr ra, a0, 0

    // Return PC from FP + 0x1234
    // Needed for branches
    ld a0, 0x100(fp)

    // Restore callee-saved registers from stack
    // TODO: handle float regs
    addi sp, sp, 200
    ld fp, 88(sp)
    ld s1, 80(sp)
    ld s2, 72(sp)
    ld s3, 64(sp)
    ld s4, 56(sp)
    ld s5, 48(sp)
    ld s6, 40(sp)
    ld s7, 32(sp)
    ld s8, 24(sp)
    ld s9, 16(sp)
    ld s10, 8(sp)
    ld s11, 0(sp)
    addi sp, sp, 94

    ret
.size call_native,.-call_native

