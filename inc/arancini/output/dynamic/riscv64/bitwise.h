#pragma once

#include <arancini/output/dynamic/riscv64/register.h>
#include <arancini/output/dynamic/riscv64/utils.h>

inline void truncate(InstructionBuilder &builder, TypedRegister &out,
                     const TypedRegister &src) {
    if (!out.type().is_vector() && out.type().is_integer()) {
        const RegisterOperand src_reg =
            is_i128_t(src.type()) ? src.reg1() : src;
        switch (out.type().element_width()) {
        case 1:
            // Flags are always zero extended
            builder.andi(out, src_reg, 1);
            out.set_type(value_type::u64());
            break;
        case 8:
        case 16:
            // No op FIXME not sure if good idea
            if (src_reg == out) {
                builder.mv(out, src_reg);
            }
            break;
        case 32:
            builder.sextw(out, src_reg);
            out.set_actual_width();
            out.set_type(value_type::u64());
            break;
        default:
            throw std::runtime_error("not implemented");
        }
    } else {
        throw std::runtime_error("not implemented");
    }
}

inline void bit_extract(InstructionBuilder &builder, TypedRegister &out,
                        const TypedRegister &src, int from, int length) {
    if (is_flag_t(out.type()) && is_gpr_t(src.type())) {
        if (from == 0) {
            builder.andi(out, src, 1);
        } else if (from == 63) {
            builder.srli(out, src, 63);
        } else if (from == 31) {
            builder.srliw(out, src, 31);
        } else {
            builder.srli(out, src, from);
            builder.andi(out, out, 1);
        }
        out.set_type(value_type::u64());
        return;
    }

    if (!(is_gpr_t(out.type()) && src.type().is_integer())) {
        throw std::runtime_error("Unsupported bit extract width.");
    }

    if (from < 64 && from + length > 64) {
        throw std::runtime_error("Register crossing bit extract unsupported.");
    }

    const RegisterOperand src1 = src.type().width() != 128 ? src
                                 : from < 64               ? src.reg1()
                                                           : src.reg2();
    if (from >= 64) {
        from -= 64;
    }

    if (from == 0 && length == 32) {
        builder.sextw(out, src1);
        out.set_actual_width(32);
        out.set_type(value_type::u64());
        return;
    }

    RegisterOperand temp = length + from < 64 ? out : src1;
    if (length + from < 64) {
        builder.slli(out, src1, 64 - (from + length));
    }
    builder.srai(out, temp,
                 64 - length); // Use arithmetic shift to keep sign extension up
    out.set_actual_width();
    out.set_type(value_type::u64());
}

inline void bit_insert(InstructionBuilder &builder, TypedRegister &out,
                       const TypedRegister &src, const TypedRegister &bits,
                       int to, const int length) {
    if (to < 64 && to + length > 64) {
        throw std::runtime_error("Register crossing bit insert unsupported.");
    }

    RegisterOperand temp_reg = builder.next_register();

    int64_t mask = ~(((1ll << length) - 1) << to);

    if (out.type().is_integer()) {
        const RegisterOperand src1 = src.type().width() != 128 ? src
                                     : to < 64                 ? src.reg1()
                                                               : src.reg2();
        const RegisterOperand out1 = out.type().width() != 128 ? out
                                     : to < 64                 ? out.reg1()
                                                               : out.reg2();
        if (to >= 64) {
            to -= 64;
        }
        // TODO Might be able to save masking of src or bits in some cases
        if (to == 0 && IsITypeImm(mask)) {
            // Since to==0 no shift necessary and just masking both is enough
            // `~mask` also fits IType since `mask` has all but lower bits set
            builder.andi(temp_reg, bits, ~mask);
            builder.andi(out1, src1, mask);
        } else {
            RegisterOperand temp_reg1 = builder.next_register();
            gen_constant(builder, mask, temp_reg1);
            builder.and_(out1, src1, temp_reg1);

            builder.slli(temp_reg, bits, 64 - length);
            if (length + to != 64) {
                builder.srli(temp_reg, temp_reg, 64 - (length + to));
            }

            // Might be able to set "bigger" out type here, but rather careful
            // by keeping intended/explicit out type
        }

        builder.or_(out1, out1, temp_reg);

    } else {
        throw std::runtime_error("not implemented");
    }
}
