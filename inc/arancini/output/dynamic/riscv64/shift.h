/**
 * All of these implementations assume that the widths of the input reg is at
 * least as big as the intended output.
 */
#pragma once

#include <arancini/ir/value-type.h>
#include <arancini/output/dynamic/riscv64/register.h>
#include <arancini/output/dynamic/riscv64/utils.h>

inline void slli(InstructionBuilder &builder, TypedRegister &out_reg,
                 const TypedRegister &src_reg, intptr_t amt) {
    switch (out_reg.type().element_width()) {
    case 128:
        builder.slli(out_reg.reg2(), src_reg.reg2(), amt);
        builder.srli(out_reg.reg1(), src_reg.reg1(), 64 - amt);
        builder.or_(out_reg.reg2(), out_reg.reg2(), out_reg.reg1());
        builder.slli(out_reg.reg1(), src_reg.reg1(), amt);
        break;
    case 64:
        builder.slli(out_reg, src_reg, amt);
        break;
    case 32:
        builder.slliw(out_reg, src_reg, amt);
        out_reg.set_actual_width();
        out_reg.set_type(value_type::u64());
        break;
    case 8:
    case 16:
        // TODO (Could possibly save 1 instruction by doing extension here (with
        // other shift amounts) instead of later)
        builder.slli(out_reg, src_reg, amt);
        break;
    default:
        throw std::runtime_error("unimplemented");
    }
}

inline void srli(InstructionBuilder &builder, TypedRegister &out_reg,
                 const TypedRegister &src_reg, intptr_t amt) {
    switch (out_reg.type().element_width()) {
    case 128:
        builder.srli(out_reg.reg1(), src_reg.reg1(), amt);
        builder.slli(out_reg.reg2(), src_reg.reg2(), 64 - amt);
        builder.or_(out_reg.reg1(), out_reg.reg1(), out_reg.reg2());
        builder.srli(out_reg.reg2(), src_reg.reg2(), amt);
        break;
    case 64:
        builder.srli(out_reg, src_reg, amt);
        break;
    case 32:
        builder.srliw(out_reg, src_reg, amt);
        out_reg.set_actual_width();
        out_reg.set_type(value_type::u64());
        break;
    case 16:
    case 8:
        if (!fixup(builder, out_reg, src_reg, value_type::u32(), -amt))
            builder.srliw(out_reg, out_reg, amt);
        break;
    default:
        throw std::runtime_error("unimplemented");
    }
}

inline void srai(InstructionBuilder &builder, TypedRegister &out_reg,
                 const TypedRegister &src_reg, intptr_t amt) {
    switch (out_reg.type().element_width()) {
    case 64:
        builder.srai(out_reg, src_reg, amt);
        break;
    case 32:
        builder.sraiw(out_reg, src_reg, amt);
        out_reg.set_actual_width();
        out_reg.set_type(value_type::u64());
        break;
    case 16:
    case 8:
        if (!fixup(builder, out_reg, src_reg, value_type::s32(), -amt))
            builder.sraiw(out_reg, out_reg, amt);
        break;
    default:
        throw std::runtime_error("unimplemented");
    }
}

inline void sll(InstructionBuilder &builder, TypedRegister &out_reg,
                const TypedRegister &src_reg, const TypedRegister &amt_reg) {
    switch (out_reg.type().element_width()) {
    case 64:
        builder.sll(out_reg, src_reg, amt_reg);
        break;
    case 32:
        builder.sllw(out_reg, src_reg, amt_reg);
        out_reg.set_actual_width();
        out_reg.set_type(value_type::u64());
        break;
    case 8:
    case 16:
        builder.sll(out_reg, src_reg, amt_reg);
        break;
    default:
        throw std::runtime_error("unimplemented");
    }
}

inline void srl(InstructionBuilder &builder, TypedRegister &out_reg,
                const TypedRegister &src_reg, const TypedRegister &amt_reg) {
    switch (out_reg.type().element_width()) {
    case 64:
        builder.srl(out_reg, src_reg, amt_reg);
        break;
    case 32:
        builder.srlw(out_reg, src_reg, amt_reg);
        out_reg.set_actual_width();
        out_reg.set_type(value_type::u64());
        break;
    case 16:
    case 8:
        fixup(builder, out_reg, src_reg, value_type::u32());
        builder.srlw(out_reg, out_reg, amt_reg);
        break;
    default:
        throw std::runtime_error("unimplemented");
    }
}

inline void sra(InstructionBuilder &builder, TypedRegister &out_reg,
                const TypedRegister &src_reg, const TypedRegister &amt_reg) {
    switch (out_reg.type().element_width()) {
    case 64:
        builder.sra(out_reg, src_reg, amt_reg);
        break;
    case 32:
        builder.sraw(out_reg, src_reg, amt_reg);
        out_reg.set_actual_width();
        out_reg.set_type(value_type::u64());
        break;
    case 16:
    case 8:
        fixup(builder, out_reg, src_reg, value_type::s32());
        builder.sraw(out_reg, out_reg, amt_reg);
        break;
    default:
        throw std::runtime_error("unimplemented");
    }
}
