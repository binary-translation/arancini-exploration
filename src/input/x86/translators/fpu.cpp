#include "arancini/ir/value-type.h"
#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/internal-function-resolver.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>
#include <xed/xed-decoded-inst-api.h>
#include <xed/xed-iclass-enum.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

#define SET_C1_BIT(bool)                                                       \
    auto status = read_reg(value_type::u16(), reg_offsets::X87_STS);           \
    status = builder().insert_bit_insert(                                      \
        status->val(), builder().insert_constant_u16(bool)->val(), 9, 1);      \
    write_reg(reg_offsets::X87_STS, status->val());

// Currently no exception (bit) creation or handeling is implemented
// TODO: FPU: Implement exception handeling

// Chapter markers refer to the respective chapter in Intel® 64 and IA-32
// Architectures Software Developer’s Manual
void fpu_translator::do_translate() {
    auto inst_class = xed_decoded_inst_get_iclass(xed_inst());

    switch (inst_class) {
    // 5.2.1 X87 FPU Data Transfer Instructions
    case XED_ICLASS_FLD: {
        // auto dst = read_operand(0); // dst always st(0)
        auto val = read_operand(1);

        // Convert to 64bit
        switch (val->val().type().width()) {
        case 32:
            val = builder().insert_bitcast(value_type::f32(), val->val());
            val = builder().insert_convert(value_type::f64(), val->val());
            break;
        case 64:
            val = builder().insert_bitcast(value_type::f64(), val->val());
            break;
        case 80: {
            // Works, but ignores the Integer bit
            val = builder().insert_bitcast(value_type::u80(), val->val());
            auto frac = builder().insert_bit_extract(val->val(), 68, 12);
            auto sign_exponent =
                builder().insert_bit_extract(val->val(), 11, 52);
            val = builder().insert_constant_f64(0);
            val = builder().insert_bit_insert(val->val(), frac->val(), 52, 12);
            val = builder().insert_bit_insert(val->val(), sign_exponent->val(),
                                              0, 52);

            // Does not work, as truncxfdf2 isn't available
            // The only implementation I could find is on m68k (motorola 68000)
            // in gcc:
            // https://github.com/gcc-mirror/gcc/blob/3e00d7dcc5b9a30d3f356369828e8ceb03de91b9/libgcc/config/m68k/fpgnulib.c#L494
            // As well as:
            // https://github.com/freemint/fdlibm/blob/master/truncxfdf2.c
            // val = builder().insert_bitcast(value_type::f80(), val->val());
            // val = builder().insert_convert(value_type::f64(), val->val());
            break;
        }
        default:
            throw std::runtime_error(
                std::string("unsupported X87 value size (only 32, 64 and 80 "
                            "bit are supported)"));
        }

        // See Intel Software developer manual: 8.5.1.1 Stack Overflow or
        // Underflow Exception (#IS)
        // TODO: FPU: Check for underflow

        // push on the stack
        fpu_push(val->val());

        break;
    }
    case XED_ICLASS_FST:
    case XED_ICLASS_FSTP: {
        // TODO: FPU: Flags

        auto dst = read_operand(0);

        // get stack top
        auto st0 = fpu_stack_get(0);

        switch (dst->val().type().width()) {
        case 32:
            st0 = builder().insert_convert(value_type::f32(), st0->val());
            write_operand(0, st0->val());
            break;
        case 64: {
            // dump_xed_encoding();
            if (is_immediate_operand(0)) {
                // FST ST(i) and FSTP ST(i)
                // Get the stack index,
                int st_idx = fpu_get_instruction_index(0);
                // TODO: FPU: Missing underflow checks etc.
                fpu_stack_set(st_idx, st0->val());
                // Set Tag value
                auto tag_val = fpu_tag_get(0);
                fpu_tag_set(st_idx, tag_val->val());
                // TODO: FPU: Set correct c1 val (We just assume no error)
                auto c1_default = builder().insert_constant_u16(0b0);
                auto status = read_reg(value_type::u16(), reg_offsets::X87_STS);
                status = builder().insert_bit_insert(status->val(),
                                                     c1_default->val(), 9, 1);
                write_reg(reg_offsets::X87_STS, status->val());
            } else {
                // Working for DD /2 and DB /7
                write_operand(0, st0->val());
                break;
            }
        }
        case 80: {
            // TODO: FPU: Missing underflow checks etc.
            st0 = builder().insert_bitcast(value_type::u64(), st0->val());
            // Grab fraction
            auto frac = builder().insert_and(
                st0->val(),
                builder().insert_constant_u64(0x000FFFFFFFFFFFFF)->val());
            frac = builder().insert_lsl(
                frac->val(), builder().insert_constant_u64(11)->val());
            frac = builder().insert_zx(value_type::u80(), frac->val());

            // Remove frac from 64bit value
            st0 = builder().insert_and(
                st0->val(),
                builder().insert_constant_u64(0xFFF0000000000000)->val());

            // Extend to u80 and shift sign & esponent into correct pos
            st0 = builder().insert_zx(value_type::u80(), st0->val());
            st0 = builder().insert_lsl(
                st0->val(), builder().insert_constant_u64(16)->val());

            // Reinsert fraction
            st0 = builder().insert_or(st0->val(), frac->val());

            write_operand(0, st0->val());
            break;
        }
        default:
            throw std::runtime_error(std::string(
                "unsupported X87 FST/FSTP value size (only 32, 64 and 80 "
                "bit are supported)"));
        }

        // POP in case of FSTP
        if (inst_class == XED_ICLASS_FSTP) {
            fpu_pop();
        }
        break;
    }
    case XED_ICLASS_FILD: {
        // auto dst = read_operand(0); // dst always st(0)
        auto val = read_operand(1);

        // Cast to signed int of correct size
        val = builder().insert_bitcast(
            value_type(value_type_class::signed_integer,
                       val->val().type().width()),
            val->val());

        // Convert to f64
        val = builder().insert_convert(value_type::f64(), val->val());

        SET_C1_BIT(0);
        // push on the stack
        fpu_push(val->val());

        break;
    }
    case XED_ICLASS_FIST:
    case XED_ICLASS_FISTP: {
        // TODO: FPU: flags
        // TODO: FPU: Exceptions, e.g.
        // floating-pointinvalid-operation(#IA)exception.
        // TODO: FPU: rounding behavior

        auto st0 = fpu_stack_get(0);
        auto val = builder().insert_convert(
            value_type(value_type_class::signed_integer, get_operand_width(0)),
            st0->val());
        write_operand(0, val->val());

        SET_C1_BIT(0);
        // POP in case of FISTP
        if (inst_class == XED_ICLASS_FISTP) {
            fpu_pop();
        }
        break;
    }
    // TODO: FPU: FBLD/FBSTP
    case XED_ICLASS_FXCH: {
        // Get the stack index
        int st_idx_i = 0;
        int st_idx_j = fpu_get_instruction_index(1);

        // Get stack, tag
        auto st0 = fpu_stack_get(st_idx_i);
        auto sti = fpu_stack_get(st_idx_j);
        auto tag0 = fpu_tag_get(st_idx_i);
        auto tagi = fpu_tag_get(st_idx_j);

        auto st0_tmp = builder().alloc_local(st0->val().type());
        auto tag0_tmp = builder().alloc_local(tag0->val().type());
        builder().insert_write_local(st0_tmp, st0->val());
        builder().insert_write_local(tag0_tmp, tag0->val());

        // Set stack, tag
        fpu_stack_set(st_idx_i, sti->val());
        fpu_stack_set(st_idx_j, builder().insert_read_local(st0_tmp)->val());
        fpu_tag_set(st_idx_i, tagi->val());
        fpu_tag_set(st_idx_j, builder().insert_read_local(tag0_tmp)->val());

        SET_C1_BIT(0);
        break;
    }
    // FCMOVcc: See cmov.cpp
    // 5.2.2 X87 FPU Basic Arithmetic Instructions
    case XED_ICLASS_FADD:
    case XED_ICLASS_FADDP:
    case XED_ICLASS_FIADD:
    case XED_ICLASS_FSUB:
    case XED_ICLASS_FSUBP:
    case XED_ICLASS_FISUB:
    case XED_ICLASS_FSUBR:
    case XED_ICLASS_FSUBRP:
    case XED_ICLASS_FISUBR:
    case XED_ICLASS_FMUL:
    case XED_ICLASS_FMULP:
    case XED_ICLASS_FIMUL:
    case XED_ICLASS_FDIV:
    case XED_ICLASS_FDIVP:
    case XED_ICLASS_FIDIV:
    case XED_ICLASS_FDIVR:
    case XED_ICLASS_FDIVRP:
    case XED_ICLASS_FIDIVR: {
        // dump_xed_encoding();

        // TODO: FPU: Flags: Check for underflow
        // TODO: FPU: Flags: Round up/down
        // TODO: FPU: Correct rounding behavior
        // TODO: FPU: Check for special value combination
        // (div with +-zero, +-infty, +-NaN etc.)

        // Load values and convert them to 64bit floats (if needed)
        // Val 0 is target
        auto val_0 = read_operand(0);
        // Val 1 is second source (may be mem)
        auto val_1 = read_operand(1);
        switch (inst_class) {
        // Convert from 32bit/64bit floats
        case XED_ICLASS_FADD:
        case XED_ICLASS_FADDP:
        case XED_ICLASS_FSUB:
        case XED_ICLASS_FSUBP:
        case XED_ICLASS_FSUBR:
        case XED_ICLASS_FSUBRP:
        case XED_ICLASS_FMUL:
        case XED_ICLASS_FMULP:
        case XED_ICLASS_FDIV:
        case XED_ICLASS_FDIVP:
        case XED_ICLASS_FDIVR:
        case XED_ICLASS_FDIVRP: {
            switch (val_1->val().type().width()) {
            case 32:
                val_1 =
                    builder().insert_bitcast(value_type::f32(), val_1->val());
                val_1 =
                    builder().insert_convert(value_type::f64(), val_1->val());
                break;
            case 64:
                val_1 =
                    builder().insert_bitcast(value_type::f64(), val_1->val());
                break;
            default:
                throw std::runtime_error(
                    std::string("unsupported X87 FADD/FSUB... float width"));
            }
            break;
        }
        // Convert from 16bit/32bit ints
        case XED_ICLASS_FIADD:
        case XED_ICLASS_FISUB:
        case XED_ICLASS_FISUBR:
        case XED_ICLASS_FIMUL:
        case XED_ICLASS_FIDIV:
        case XED_ICLASS_FIDIVR: {
            switch (val_1->val().type().width()) {
            case 16:
                val_1 =
                    builder().insert_bitcast(value_type::s16(), val_1->val());
                break;
            case 32:
                val_1 =
                    builder().insert_bitcast(value_type::s32(), val_1->val());
                break;
            default:
                printf("Int width: %i\n", (int)val_1->val().type().width());
                throw std::runtime_error(
                    std::string("unsupported X87 FIADD/FISUB... int width"));
            }
            val_1 = builder().insert_convert(value_type::f64(), val_1->val());
            break;
        }
        default:
            throw std::runtime_error(
                std::string("unsupported X87 FADD/FSUB... instruction"));
        }

        // Do caluclation
        switch (inst_class) {
        case XED_ICLASS_FADD:
        case XED_ICLASS_FADDP:
        case XED_ICLASS_FIADD:
            val_0 = builder().insert_add(val_0->val(), val_1->val());
            break;
        case XED_ICLASS_FSUB:
        case XED_ICLASS_FSUBP:
        case XED_ICLASS_FISUB:
            val_0 = builder().insert_sub(val_0->val(), val_1->val());
            break;
        case XED_ICLASS_FSUBR:
        case XED_ICLASS_FSUBRP:
        case XED_ICLASS_FISUBR:
            val_0 = builder().insert_sub(val_1->val(), val_0->val());
            break;
        case XED_ICLASS_FMUL:
        case XED_ICLASS_FMULP:
        case XED_ICLASS_FIMUL:
            val_0 = builder().insert_mul(val_0->val(), val_1->val());
            break;
        case XED_ICLASS_FDIV:
        case XED_ICLASS_FDIVP:
        case XED_ICLASS_FIDIV:
            val_0 = builder().insert_div(val_0->val(), val_1->val());
            break;
        case XED_ICLASS_FDIVR:
        case XED_ICLASS_FDIVRP:
        case XED_ICLASS_FIDIVR:
            val_0 = builder().insert_div(val_1->val(), val_0->val());
            break;
        default:
            throw std::runtime_error(
                std::string("unsupported X87 FADD/FSUB... instruction2"));
        }

        // Write result
        auto idx = fpu_get_instruction_index(0);
        fpu_stack_set(idx, val_0->val());

        // TODO: FPU: Write correct TAG, catch 0, NaN, denormalised, Infinity
        fpu_tag_set(idx, builder().insert_constant_u16(0b00)->val());

        // POP if needed
        switch (inst_class) {
        case XED_ICLASS_FADDP:
        case XED_ICLASS_FSUBP:
        case XED_ICLASS_FSUBRP:
        case XED_ICLASS_FMULP:
        case XED_ICLASS_FDIVP:
        case XED_ICLASS_FDIVRP:
            fpu_pop();
            break;
        default:
            break;
        }

        // TODO: FPU: Set correct C1 bit
        SET_C1_BIT(0);
        break;
    }
    case XED_ICLASS_FPREM: {
        // xed encoding: fprem st(0) st(1)
        auto st0 = read_operand(0);
        auto st1 = read_operand(1);

        auto res = builder().insert_mod(st0->val(), st1->val());

        write_operand(0, res->val());

        // TODO: FPU: flags
        // TODO: FPU: Correct tag
        break;
    }
    case XED_ICLASS_FPREM1: {
        // xed encoding: fprem st(0) st(1)
        auto st0 = read_operand(0);
        auto st1 = read_operand(1);

        // res_1 and res_2 are two remainders that are the closest
        // to 0 (above and below 0)
        auto res_1 = builder().insert_mod(st0->val(), st1->val());
        auto res_2 = builder().insert_sub(res_1->val(), st1->val());

        // Calculate absolute values
        auto mask = builder().insert_constant_u64(0x7FFFFFFFFFFFFFFF);
        auto abs_res_1 = builder().insert_and(res_1->val(), mask->val());
        auto abs_res_2 = builder().insert_and(res_2->val(), mask->val());

        // Pick the closer value to 0 (the smaller value)
        auto res = builder().insert_csel(
            builder().insert_cmpgt(abs_res_1->val(), abs_res_2->val())->val(),
            res_2->val(), res_1->val());

        write_operand(0, res->val());

        // TODO: FPU: flags
        // TODO: FPU: Correct tag
        break;
    }
    case XED_ICLASS_FABS: {
        auto mask = builder().insert_constant_u64(0x7FFFFFFFFFFFFFFF);
        auto st0 = read_operand(0);
        st0 = builder().insert_and(st0->val(), mask->val());
        write_operand(0, st0->val());

        SET_C1_BIT(0);
        break;
    }
    case XED_ICLASS_FCHS: {
        // xed encoding: fchs st(0)
        auto mask = builder().insert_constant_u64(0x7FFFFFFFFFFFFFFF);
        auto mask_inv = builder().insert_constant_u64(0x8000000000000000);

        auto st0 = read_operand(0);

        // Value without sign
        auto res = builder().insert_and(st0->val(), mask->val());
        // Negated sign
        auto neg_sign = builder().insert_and(
            builder().insert_not(st0->val())->val(), mask_inv->val());
        // Add negated sign back to value
        res = builder().insert_or(res->val(), neg_sign->val());

        write_operand(0, res->val());

        SET_C1_BIT(0);
        break;
    }
    case XED_ICLASS_FRNDINT: {
        // TODO: FPU: Warning: Currently only rounds towards zero
        // TODO: FPU: Respect RC field (rounding mode)
        // TODO: FPU: Set correct tag in case of rounding to 0
        // TODO: FPU: Handle Underflow

        auto st0 = read_operand(0);
        auto st0_as_int =
            builder().insert_convert(value_type::s128(), st0->val());
        write_operand(0,
                      builder()
                          .insert_convert(value_type::f64(), st0_as_int->val())
                          ->val());
        break;
    }
    case XED_ICLASS_FSCALE: {
        // TODO: FPU: Set correct tag in case of special values
        // TODO: FPU: Set correct C1
        // TODO: FPU: Handle denormal values correctly
        // TODO: FPU: Handle Underflow

        auto st0 = builder().insert_bitcast(value_type::u64(),
                                            fpu_stack_get(0)->val());
        auto st1 = builder().insert_convert(value_type::u64(),
                                            fpu_stack_get(1)->val());

        // Extract exponent (we can't shift st1, as the value may be negative)
        auto exponent = builder().insert_lsr(
            st0->val(), builder().insert_constant_u64(52)->val());
        auto mask = builder().insert_constant_u64(0x7FF);
        exponent = builder().insert_and(exponent->val(), mask->val());

        // Add value to exponent as the "scale" operation
        exponent = builder().insert_add(exponent->val(), st1->val());

        // Shift it back into place
        exponent = builder().insert_lsl(
            exponent->val(), builder().insert_constant_u64(52)->val());

        // Bolt new exponent onto value
        auto exponent_mask = builder().insert_constant_u64(0x7FF0000000000000);
        auto exponent_mask_inv =
            builder().insert_constant_u64(0x800FFFFFFFFFFFFF);
        exponent = builder().insert_and(exponent->val(), exponent_mask->val());
        st0 = builder().insert_and(st0->val(), exponent_mask_inv->val());
        auto res = builder().insert_or(exponent->val(), st0->val());

        fpu_stack_set(0, res->val());
        break;
    }
    case XED_ICLASS_FSQRT: {
        // TODO: FPU: Set correct tag in case of special values
        // TODO: FPU: Set correct C1

        auto st0 = fpu_stack_get(0);
        st0 = builder().insert_sqrt(st0->val());
        fpu_stack_set(0, st0->val());
        break;
    }
    case XED_ICLASS_FXTRACT: {
        // TODO: FPU: Set correct tag in case of special values
        // TODO: FPU: Set correct C1
        // TODO: FPU: Handle special values correctly
        // TODO: FPU: Handle Underflow

        auto st0 = builder().insert_bitcast(value_type::u64(),
                                            fpu_stack_get(0)->val());

        // Get exponent value
        auto exponent = builder().insert_lsr(
            st0->val(), builder().insert_constant_u64(52)->val());
        auto mask = builder().insert_constant_u64(0x7FF);
        exponent = builder().insert_and(exponent->val(), mask->val());
        // Remove bias
        exponent = builder().insert_sub(
            exponent->val(), builder().insert_constant_u64(1023)->val());
        exponent = builder().insert_convert(value_type::f64(), exponent->val());

        // Scale signigicand to true zero
        auto true_zero_exponent_value =
            builder().insert_constant_u64(0x3FF0000000000000);
        auto true_zero_exponent_mask =
            builder().insert_constant_u64(0x800FFFFFFFFFFFFF);
        auto significand = builder().insert_or(
            builder()
                .insert_and(st0->val(), true_zero_exponent_mask->val())
                ->val(),
            true_zero_exponent_value->val());

        fpu_stack_set(0, exponent->val());
        fpu_push(significand->val());
        break;
    }
    // 5.2.3 X87 FPU Comparison Instructions
    case XED_ICLASS_FCOM:
    case XED_ICLASS_FCOMP:
    case XED_ICLASS_FCOMPP:
    case XED_ICLASS_FUCOM:
    case XED_ICLASS_FUCOMP:
    case XED_ICLASS_FUCOMPP:
    case XED_ICLASS_FICOM:
    case XED_ICLASS_FICOMP:
    // FCOMI/FUCOMI/FCOMIP/FUCOMIP see below
    case XED_ICLASS_FTST: {
        // TODO: FPU: Check for underflow
        // TODO: FPU: properly manage the unordered case
        // TODO: FPU: Handle differences of FUCOM/FUCOMP/FUCOMPP
        // dump_xed_encoding();

        // Load values and convert them to 64bit floats (if needed)
        auto st0 = fpu_stack_get(0);
        // Val is first source (may be mem)
        value_node *src;
        if (inst_class != XED_ICLASS_FTST) {
            // Standard FCOM/FCOMP... load value
            src = read_operand(1);
        } else {
            src = builder().insert_constant_f64(0.0f);
        }

        switch (inst_class) {
        // Convert from 32bit/64bit floats
        case XED_ICLASS_FCOM:
        case XED_ICLASS_FCOMP:
        case XED_ICLASS_FCOMPP:
        case XED_ICLASS_FUCOM:
        case XED_ICLASS_FUCOMP:
        case XED_ICLASS_FUCOMPP: {
            switch (src->val().type().width()) {
            case 32:
                src = builder().insert_bitcast(value_type::f32(), src->val());
                src = builder().insert_convert(value_type::f64(), src->val());
                break;
            case 64:
                src = builder().insert_bitcast(value_type::f64(), src->val());
                break;
            default:
                throw std::runtime_error(
                    std::string("unsupported X87 FCOM/FCOMP... float width"));
            }
            break;
        }
        // Convert from 16bit/32bit ints
        case XED_ICLASS_FICOM:
        case XED_ICLASS_FICOMP: {
            switch (src->val().type().width()) {
            case 16:
                src = builder().insert_bitcast(value_type::s16(), src->val());
                break;
            case 32:
                src = builder().insert_bitcast(value_type::s32(), src->val());
                break;
            default:
                printf("Int width: %i\n", (int)src->val().type().width());
                throw std::runtime_error(
                    std::string("unsupported X87 FICOM/FICOMP int width"));
            }
            src = builder().insert_convert(value_type::f64(), src->val());
            break;
        }
        case XED_ICLASS_FTST: {
            break;
        }
        default:
            throw std::runtime_error(
                std::string("unsupported X87 FCOM/FCOMP... instruction"));
        }

        // Assume ST(0) > SRC therefore setting (C3=C2=C1=C0=0)
        auto sts = read_reg(value_type::u16(), reg_offsets::X87_STS);
        sts = builder().insert_and(
            sts->val(), builder().insert_constant_u16(0xB8FF)->val());

        // Test ST(0) < SRC (If true set C0=1)
        auto c0 = builder().insert_zx(
            value_type::u16(),
            builder().insert_cmpgt(src->val(), st0->val())->val());
        sts = builder().insert_bit_insert(sts->val(), c0->val(), 8, 1);

        // Test ST(0) = SRC (If true set C3=1)
        auto c3 = builder().insert_zx(
            value_type::u16(),
            builder().insert_cmpeq(src->val(), st0->val())->val());
        sts = builder().insert_bit_insert(sts->val(), c3->val(), 14, 1);

        write_reg(reg_offsets::X87_STS, sts->val());

        // Pop if required
        switch (inst_class) {
        case XED_ICLASS_FCOMPP:
        case XED_ICLASS_FUCOMPP:
            fpu_pop();
        case XED_ICLASS_FCOMP:
        case XED_ICLASS_FUCOMP:
        case XED_ICLASS_FICOMP:
            fpu_pop();
            break;
        default:
            break;
        }
        break;
    }
    case XED_ICLASS_FCOMI:
    case XED_ICLASS_FCOMIP:
    case XED_ICLASS_FUCOMI:
    case XED_ICLASS_FUCOMIP: {
        // TODO: FPU: Check for underflow
        // TODO: FPU: properly manage the unordered case (SNan, QNaN, etc), and
        // diff with F* and FU*
        // xed encoding: fucomi(p) st(0), st(i)

        // get stack top and operand
        auto st0 = read_operand(0);
        auto sti = read_operand(1);

        // Test ST(0) = SRC (If true set ZF=1)
        auto zf = builder().insert_cmpeq(sti->val(), st0->val());
        // Test ST(0) < SRC (If true set CF=1)
        auto cf = builder().insert_cmpgt(sti->val(), st0->val());
        auto pf = builder().insert_constant_i(value_type::u1(), 0);

        write_reg(reg_offsets::ZF, zf->val());
        write_reg(reg_offsets::CF, cf->val());
        write_reg(reg_offsets::PF, pf->val());

        // Pop if required
        switch (inst_class) {
        case XED_ICLASS_FCOMIP:
        case XED_ICLASS_FUCOMIP:
            fpu_pop();
        default:
            break;
        }
        break;
    }
    case XED_ICLASS_FXAM: {
        // TODO: FPU: Unsupported Value

        // Clear C0-C4 bits
        auto sts = read_reg(value_type::u16(), reg_offsets::X87_STS);
        sts = builder().insert_and(
            sts->val(), builder().insert_constant_u16(0xB8FF)->val());

        // Set C1 bit to sign of ST(0)
        auto st0 = fpu_stack_get(0);
        st0 = builder().insert_bitcast(value_type::u64(), st0->val());
        // Shift sign into the position into C1 bit pos
        auto sign = builder().insert_lsr(
            st0->val(), builder().insert_constant_u16(54)->val());
        sign = builder().insert_trunc(value_type::u16(), sign->val());
        // Bolt onto sts
        sign = builder().insert_and(
            sign->val(), builder().insert_constant_u16(0x0200)->val());
        auto sts_with_c1 = builder().insert_or(sts->val(), sign->val());

        // Needed basevalues for comparisons
        // Mantissa
        auto mantissa = builder().insert_and(
            st0->val(), builder().insert_constant_u64(0xFFFFFFFFFFFFF)->val());
        // Exponent (not shifted into place)
        auto exponent = builder().insert_and(
            st0->val(),
            builder().insert_constant_u64(0x7FF0000000000000)->val());

        // Needed conditionals
        // True, if mantissa is 0
        auto bool_mantissa_0 = builder().insert_cmpeq(
            mantissa->val(), builder().insert_constant_u64(0)->val());
        // True, if mantissa is NOT 0
        auto bool_mantissa_n0 = builder().insert_not(bool_mantissa_0->val());
        // True, if exponent is 0
        auto bool_exponent_0 = builder().insert_cmpeq(
            exponent->val(), builder().insert_constant_u64(0)->val());
        // True, if exponent is 7FF (aka. filled with 1s)
        auto bool_exponent_F = builder().insert_cmpeq(
            exponent->val(),
            builder().insert_constant_u64(0x7FF0000000000000)->val());

        // Define default C3,C2,C0 Values (Normal finitie number)
        // c_vals represents the current assignment
        auto c_vals_normal = builder().insert_constant_u16(0x400);

        // Check for zero
        auto bool_zero = builder().insert_and(bool_exponent_0->val(),
                                              bool_mantissa_0->val());
        auto c_vals_zero = builder().insert_csel(
            bool_zero->val(), builder().insert_constant_u16(0x4000)->val(),
            c_vals_normal->val());

        // Check for infinity
        auto bool_infty = builder().insert_and(bool_exponent_F->val(),
                                               bool_mantissa_0->val());
        auto c_vals_infty = builder().insert_csel(
            bool_infty->val(), builder().insert_constant_u16(0x500)->val(),
            c_vals_zero->val());

        // Check for denormalised
        auto bool_denormalised = builder().insert_and(bool_exponent_0->val(),
                                                      bool_mantissa_n0->val());
        auto c_vals_denormalised = builder().insert_csel(
            bool_denormalised->val(),
            builder().insert_constant_u16(0x4400)->val(), c_vals_infty->val());

        // Check if NaN
        auto bool_nan = builder().insert_and(bool_exponent_F->val(),
                                             bool_mantissa_n0->val());
        auto c_vals_nan = builder().insert_csel(
            bool_nan->val(), builder().insert_constant_u16(0x100)->val(),
            c_vals_denormalised->val());

        // Check if st0 is empty
        auto st0_tag = fpu_tag_get(0);
        auto bool_empty = builder().insert_cmpeq(
            st0_tag->val(), builder().insert_constant_u16(0b11)->val());
        auto c_vals = builder().insert_csel(
            bool_empty->val(), builder().insert_constant_u16(0x4100)->val(),
            c_vals_nan->val());

        // Add c_vals to status
        auto sts_result =
            builder().insert_or(sts_with_c1->val(), c_vals->val());

        write_reg(reg_offsets::X87_STS, sts_result->val());
        break;
    }

    // case XED_ICLASS_FLDZ: {
    //     auto zero = builder().insert_constant_f(
    //         value_type(value_type_class::floating_point, 80), +0.0);
    //     fpu_stack_top_move(-1);
    //     fpu_stack_set(0, zero->val());

    //     // TODO FPU flags
    //     break;
    // }
    // case XED_ICLASS_FLD1: {
    //     auto one = builder().insert_constant_f(
    //         value_type(value_type_class::floating_point, 80), +1.0);
    //     fpu_stack_top_move(-1);
    //     fpu_stack_set(0, one->val());

    //     // TODO FPU flags
    //     break;
    // }
    // case XED_ICLASS_FNSTCW: {
    //     auto fpu_ctrl = read_reg(value_type::u16(), reg_offsets::X87_CTRL);
    //     write_operand(0, fpu_ctrl->val());
    //     break;
    // }
    // case XED_ICLASS_FLDCW: {
    //     auto src = read_operand(0);
    //     write_reg(reg_offsets::X87_CTRL, src->val());
    //     break;
    // }
    // case XED_ICLASS_FNSTSW: {
    //     auto fpu_status = read_reg(value_type::u16(), reg_offsets::X87_STS);
    //     write_operand(0, fpu_status->val());
    //     break;
    // }
    default:
        throw std::runtime_error(
            std::string("unsupported fpu operation") +
            xed_iclass_enum_t2str(xed_decoded_inst_get_iclass(xed_inst())));
    }

    // switch (inst_class) {
    // case XED_ICLASS_FSTP:
    // case XED_ICLASS_FISTP:
    // case XED_ICLASS_FADDP:
    // case XED_ICLASS_FSUBP:
    // case XED_ICLASS_FSUBRP:
    // case XED_ICLASS_FMULP:
    // case XED_ICLASS_FDIVP:
    // case XED_ICLASS_FDIVRP:
    // case XED_ICLASS_FCOMIP:
    // case XED_ICLASS_FUCOMIP:
    //     fpu_stack_top_move(1);
    //     break;
    // default:
    //     break;
    // }
}
