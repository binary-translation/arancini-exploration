#include "arancini/ir/value-type.h"
#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/internal-function-resolver.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>
#include <xed/xed-decoded-inst-api.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

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
                // Get the stack index, this is the same code as in
                // read_operand(0)
                const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst());
                auto operand = xed_inst_operand(insn, 0);
                auto opname = xed_operand_name(operand);
                auto reg = xed_decoded_inst_get_reg(xed_inst(), opname);
                int st_idx = reg - XED_REG_ST0;
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

        // POP in case of FISTP
        if (inst_class == XED_ICLASS_FISTP) {
            fpu_pop();
        }
        break;
    }
    // case XED_ICLASS_FILD: {
    //     // xed encoding: fild st0 memint
    //     auto val = read_operand(1);

    //     if (val->val().type().width() != 80) {
    //         val = builder().insert_convert(value_type::f80(), val->val());
    //     }

    //     fpu_stack_top_move(-1);
    //     fpu_stack_set(0, val->val());

    //     // TODO FPU flags
    //     break;
    // }
    // case XED_ICLASS_FIMUL: {
    //     auto dst = fpu_stack_get(0);
    //     auto src = read_operand(1);

    //     if (src->val().type().width() != 80) {
    //         src = builder().insert_convert(dst->val().type(), src->val());
    //     }

    //     auto res = builder().insert_mul(src->val(), dst->val());

    //     fpu_stack_set(0, res->val());
    //     // TODO FPU flags
    //     break;
    // }
    // case XED_ICLASS_FADD:
    // case XED_ICLASS_FADDP: {
    //     // xed encoding: fadd st(0) st(i)
    //     auto dst = read_operand(0);
    //     auto src = read_operand(1);

    //     if (src->val().type().width() == 32)
    //         src = builder().insert_bitcast(value_type::f32(), src->val());
    //     else if (src->val().type().width() == 64)
    //         src = builder().insert_bitcast(value_type::f64(), src->val());

    //     if (src->val().type().width() != 80) {
    //         src = builder().insert_convert(dst->val().type(), src->val());
    //     }

    //     auto res = builder().insert_add(dst->val(), src->val());

    //     write_operand(0, res->val());

    //     // TODO FPU flags
    //     break;
    // }
    // case XED_ICLASS_FSUB:
    // case XED_ICLASS_FSUBP: {
    //     // xed encoding: fsub st(0) st(i)
    //     auto dst = read_operand(0);
    //     auto src = read_operand(1);

    //     if (src->val().type().width() == 32)
    //         src = builder().insert_bitcast(value_type::f32(), src->val());
    //     else if (src->val().type().width() == 64)
    //         src = builder().insert_bitcast(value_type::f64(), src->val());

    //     if (src->val().type().width() != 80) {
    //         src = builder().insert_convert(dst->val().type(), src->val());
    //     }

    //     auto res = builder().insert_sub(dst->val(), src->val());

    //     write_operand(0, res->val());

    //     // TODO FPU flags
    //     break;
    // }
    // case XED_ICLASS_FSUBR:
    // case XED_ICLASS_FSUBRP: {
    //     auto dst = read_operand(0);
    //     auto src = read_operand(1);

    //     if (src->val().type().width() == 32)
    //         src = builder().insert_bitcast(value_type::f32(), src->val());
    //     else if (src->val().type().width() == 64)
    //         src = builder().insert_bitcast(value_type::f64(), src->val());

    //     if (src->val().type().width() != 80) {
    //         src = builder().insert_convert(dst->val().type(), src->val());
    //     }

    //     auto res = builder().insert_sub(src->val(), dst->val());

    //     write_operand(0, res->val());

    //     // TODO FPU flags
    //     break;
    // }
    // case XED_ICLASS_FIADD:
    // case XED_ICLASS_FISUB: {
    //     // xed encoding: fisub st(0) memint
    //     auto st0 = read_operand(0);
    //     auto op = read_operand(1);
    //     auto conv = builder().insert_convert(st0->val().type(), op->val());

    //     value_node *res;
    //     switch (inst_class) {
    //     case XED_ICLASS_FISUB:
    //         res = builder().insert_sub(st0->val(), conv->val());
    //         break;
    //     case XED_ICLASS_FIADD:
    //         res = builder().insert_add(st0->val(), conv->val());
    //         break;
    //     default:
    //         res = nullptr;
    //         break;
    //     }

    //     write_operand(0, res->val());

    //     // TODO FPU flags
    //     break;
    // }
    // case XED_ICLASS_FRNDINT: {
    //     auto st0 = read_operand(0);
    //     auto width = st0->val().type().width();
    //     value_type toint = value_type::s64();
    //     if (width == 32)
    //         toint = value_type::s32();

    //     auto conv = builder().insert_convert(toint, st0->val());
    //     write_operand(
    //         0, builder().insert_convert(st0->val().type(),
    //         conv->val())->val());
    //     break;
    // }
    // case XED_ICLASS_FMUL:
    // case XED_ICLASS_FMULP: {
    //     // xed encoding: fmul st(i) st(j)
    //     auto dst = read_operand(0);
    //     auto src = read_operand(1);

    //     if (src->val().type().width() == 32)
    //         src = builder().insert_bitcast(value_type::f32(), src->val());
    //     else if (src->val().type().width() == 64)
    //         src = builder().insert_bitcast(value_type::f64(), src->val());

    //     if (src->val().type().width() != 80) {
    //         src = builder().insert_convert(dst->val().type(), src->val());
    //     }

    //     auto res = builder().insert_mul(dst->val(), src->val());
    //     write_operand(0, res->val());

    //     // TODO FPU flags
    //     break;
    // }
    // case XED_ICLASS_FDIV:
    // case XED_ICLASS_FDIVP: {
    //     // xed encoding: fdiv st(i) st(j)
    //     auto dst = read_operand(0);
    //     auto src = read_operand(1);

    //     if (src->val().type().width() == 32)
    //         src = builder().insert_bitcast(value_type::f32(), src->val());
    //     else if (src->val().type().width() == 64)
    //         src = builder().insert_bitcast(value_type::f64(), src->val());

    //     if (src->val().type().width() != 80) {
    //         src = builder().insert_convert(dst->val().type(), src->val());
    //     }

    //     auto res = builder().insert_div(dst->val(), src->val());
    //     write_operand(0, res->val());

    //     // TODO FPU flags
    //     break;
    // }
    // case XED_ICLASS_FDIVR:
    // case XED_ICLASS_FDIVRP: {
    //     // xed encoding: fdiv st(i) st(j)
    //     auto src = read_operand(0);
    //     auto dst = read_operand(1);

    //     if (src->val().type().width() == 32)
    //         src = builder().insert_bitcast(value_type::f32(), src->val());
    //     else if (src->val().type().width() == 64)
    //         src = builder().insert_bitcast(value_type::f64(), src->val());

    //     if (src->val().type().width() != 80) {
    //         src = builder().insert_convert(dst->val().type(), src->val());
    //     }

    //     auto res = builder().insert_div(dst->val(), src->val());
    //     write_operand(0, res->val());

    //     // TODO FPU flags
    //     break;
    // }
    // case XED_ICLASS_FPREM: {
    //     // xed encoding: fprem st(0) st(1)
    //     auto st0 = read_operand(0);
    //     auto st1 = read_operand(1);

    //     auto res = builder().insert_mod(st0->val(), st1->val());

    //     write_operand(0, res->val());

    //     // TODO FPU flags
    //     break;
    // }
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
    // case XED_ICLASS_FCHS: {
    //     // xed encoding: fchs st(0)
    //     auto st0 = read_operand(0);
    //     write_operand(0, builder().insert_not(st0->val())->val());

    //     // TODO flag management
    //     break;
    // }
    // case XED_ICLASS_FABS: {
    //     auto st0 = read_operand(0);
    //     st0 = builder().insert_bit_insert(
    //         st0->val(), builder().insert_constant_u1(1)->val(), 79, 1);
    //     write_operand(0, st0->val());

    //     // TODO set C1 flag to 0
    //     break;
    // }
    // case XED_ICLASS_FXCH: {
    //     // xed encoding: fxch st(i) st(j)
    //     auto sti = read_operand(0);
    //     auto stj = read_operand(1);
    //     auto tmp = builder().alloc_local(sti->val().type());
    //     builder().insert_write_local(tmp, sti->val());

    //     write_operand(0, stj->val());
    //     write_operand(1, builder().insert_read_local(tmp)->val());

    //     // TODO set C1 flag to 0
    //     break;
    // }
    // case XED_ICLASS_FCOMI:
    // case XED_ICLASS_FCOMIP:
    // case XED_ICLASS_FUCOMI:
    // case XED_ICLASS_FUCOMIP: {
    //     // xed encoding: fucomi(p) st(0), st(i)
    //     // TODO properly manage the unordered case (SNan, QNaN, etc), and
    //     diff
    //     // with F* and FU*

    //     // get stack top and operand
    //     auto st0 = read_operand(0);
    //     auto sti = read_operand(1);

    //     // comparisons
    //     st0 = builder().insert_convert(value_type::f64(), st0->val());
    //     sti = builder().insert_convert(value_type::f64(), sti->val());

    //     auto cmplt = builder().insert_binop(binary_arith_op::cmpolt,
    //     st0->val(),
    //                                         sti->val());
    //     auto cmpeq = builder().insert_binop(binary_arith_op::cmpoeq,
    //     sti->val(),
    //                                         st0->val());

    //     auto zf = builder().insert_csel(
    //         cmpeq->val(),
    //         builder().insert_constant_i(value_type::u1(), 1)->val(),
    //         builder().insert_constant_i(value_type::u1(), 0)->val());
    //     auto cf = builder().insert_csel(
    //         cmplt->val(),
    //         builder().insert_constant_i(value_type::u1(), 1)->val(),
    //         builder().insert_constant_i(value_type::u1(), 0)->val());
    //     auto pf = builder().insert_constant_i(value_type::u1(), 0);

    //     builder().insert_write_reg(util::to_underlying(reg_offsets::ZF),
    //                                util::to_underlying(reg_idx::ZF), "ZF",
    //                                zf->val());
    //     builder().insert_write_reg(util::to_underlying(reg_offsets::CF),
    //                                util::to_underlying(reg_idx::CF), "CF",
    //                                cf->val());
    //     builder().insert_write_reg(util::to_underlying(reg_offsets::PF),
    //                                util::to_underlying(reg_idx::PF), "PF",
    //                                pf->val());

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
