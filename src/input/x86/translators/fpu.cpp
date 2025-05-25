#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/internal-function-resolver.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void fpu_translator::do_translate() {
    auto inst_class = xed_decoded_inst_get_iclass(xed_inst());

    switch (inst_class) {
    case XED_ICLASS_FNSTCW: {
        auto fpu_ctrl = read_reg(value_type::u16(), reg_offsets::X87_CTRL);
        write_operand(0, fpu_ctrl->val());
        break;
    }
    case XED_ICLASS_FLDCW: {
        auto src = read_operand(0);
        write_reg(reg_offsets::X87_CTRL, src->val());
        break;
    }
    case XED_ICLASS_FNSTSW: {
        auto fpu_status = read_reg(value_type::u16(), reg_offsets::X87_STS);
        write_operand(0, fpu_status->val());
        break;
    }
    case XED_ICLASS_FLD: {
        auto dst = read_operand(0); // always st(0)
        auto val = read_operand(1);
        if (val->val().type().width() == 32)
            val = builder().insert_bitcast(value_type::f32(), val->val());
        else if (val->val().type().width() == 64)
            val = builder().insert_bitcast(value_type::f64(), val->val());

        if (val->val().type().width() != 80) {
            val = builder().insert_convert(value_type::f80(), val->val());
        }

        // push on the stack
        fpu_stack_top_move(-1);
        fpu_stack_set(0, val->val());

        // TODO flag management

        break;
    }
    case XED_ICLASS_FILD: {
        // xed encoding: fild st0 memint
        auto val = read_operand(1);

        if (val->val().type().width() != 80) {
            val = builder().insert_convert(value_type::f80(), val->val());
        }

        fpu_stack_top_move(-1);
        fpu_stack_set(0, val->val());

        // TODO FPU flags
        break;
    }
    case XED_ICLASS_FIMUL: {
        auto dst = fpu_stack_get(0);
        auto src = read_operand(1);

        if (src->val().type().width() != 80) {
            src = builder().insert_convert(dst->val().type(), src->val());
        }

        auto res = builder().insert_mul(src->val(), dst->val());

        fpu_stack_set(0, res->val());
        // TODO FPU flags
        break;
    }
    case XED_ICLASS_FST:
    case XED_ICLASS_FSTP: {
        auto target_width = get_operand_width(0);

        // get stack top
        auto val = fpu_stack_get(0);

        // convert and write to memory
        if (target_width != 80) {
            val = builder().insert_convert(
                value_type(value_type_class::floating_point, target_width),
                val->val());
        }
        write_operand(0, val->val());

        // TODO flag management
        break;
    }
    case XED_ICLASS_FIST:
    case XED_ICLASS_FISTP: {
        auto st0 = fpu_stack_get(0);
        auto val = builder().insert_convert(
            value_type(value_type_class::signed_integer, get_operand_width(0)),
            st0->val());
        write_operand(0, val->val());

        // TODO FPU flags
        break;
    }
    case XED_ICLASS_FADD:
    case XED_ICLASS_FADDP: {
        // xed encoding: fadd st(0) st(i)
        auto dst = read_operand(0);
        auto src = read_operand(1);

        if (src->val().type().width() == 32)
            src = builder().insert_bitcast(value_type::f32(), src->val());
        else if (src->val().type().width() == 64)
            src = builder().insert_bitcast(value_type::f64(), src->val());

        if (src->val().type().width() != 80) {
            src = builder().insert_convert(dst->val().type(), src->val());
        }

        auto res = builder().insert_add(dst->val(), src->val());

        write_operand(0, res->val());

        // TODO FPU flags
        break;
    }
    case XED_ICLASS_FSUB:
    case XED_ICLASS_FSUBP: {
        // xed encoding: fsub st(0) st(i)
        auto dst = read_operand(0);
        auto src = read_operand(1);

        if (src->val().type().width() == 32)
            src = builder().insert_bitcast(value_type::f32(), src->val());
        else if (src->val().type().width() == 64)
            src = builder().insert_bitcast(value_type::f64(), src->val());

        if (src->val().type().width() != 80) {
            src = builder().insert_convert(dst->val().type(), src->val());
        }

        auto res = builder().insert_sub(dst->val(), src->val());

        write_operand(0, res->val());

        // TODO FPU flags
        break;
    }
    case XED_ICLASS_FSUBR:
    case XED_ICLASS_FSUBRP: {
        auto dst = read_operand(0);
        auto src = read_operand(1);

        if (src->val().type().width() == 32)
            src = builder().insert_bitcast(value_type::f32(), src->val());
        else if (src->val().type().width() == 64)
            src = builder().insert_bitcast(value_type::f64(), src->val());

        if (src->val().type().width() != 80) {
            src = builder().insert_convert(dst->val().type(), src->val());
        }

        auto res = builder().insert_sub(src->val(), dst->val());

        write_operand(0, res->val());

        // TODO FPU flags
        break;
    }
    case XED_ICLASS_FIADD:
    case XED_ICLASS_FISUB: {
        // xed encoding: fisub st(0) memint
        auto st0 = read_operand(0);
        auto op = read_operand(1);
        auto conv = builder().insert_convert(st0->val().type(), op->val());

        value_node *res;
        switch (inst_class) {
        case XED_ICLASS_FISUB:
            res = builder().insert_sub(st0->val(), conv->val());
            break;
        case XED_ICLASS_FIADD:
            res = builder().insert_add(st0->val(), conv->val());
            break;
        default:
            res = nullptr;
            break;
        }

        write_operand(0, res->val());

        // TODO FPU flags
        break;
    }
    case XED_ICLASS_FRNDINT: {
        auto st0 = read_operand(0);
        auto width = st0->val().type().width();
        value_type toint = value_type::s64();
        if (width == 32)
            toint = value_type::s32();

        auto conv = builder().insert_convert(toint, st0->val());
        write_operand(
            0, builder().insert_convert(st0->val().type(), conv->val())->val());
        break;
    }
    case XED_ICLASS_FMUL:
    case XED_ICLASS_FMULP: {
        // xed encoding: fmul st(i) st(j)
        auto dst = read_operand(0);
        auto src = read_operand(1);

        if (src->val().type().width() == 32)
            src = builder().insert_bitcast(value_type::f32(), src->val());
        else if (src->val().type().width() == 64)
            src = builder().insert_bitcast(value_type::f64(), src->val());

        if (src->val().type().width() != 80) {
            src = builder().insert_convert(dst->val().type(), src->val());
        }

        auto res = builder().insert_mul(dst->val(), src->val());
        write_operand(0, res->val());

        // TODO FPU flags
        break;
    }
    case XED_ICLASS_FDIV:
    case XED_ICLASS_FDIVP: {
        // xed encoding: fdiv st(i) st(j)
        auto dst = read_operand(0);
        auto src = read_operand(1);

        if (src->val().type().width() == 32)
            src = builder().insert_bitcast(value_type::f32(), src->val());
        else if (src->val().type().width() == 64)
            src = builder().insert_bitcast(value_type::f64(), src->val());

        if (src->val().type().width() != 80) {
            src = builder().insert_convert(dst->val().type(), src->val());
        }

        auto res = builder().insert_div(dst->val(), src->val());
        write_operand(0, res->val());

        // TODO FPU flags
        break;
    }
    case XED_ICLASS_FDIVR:
    case XED_ICLASS_FDIVRP: {
        // xed encoding: fdiv st(i) st(j)
        auto src = read_operand(0);
        auto dst = read_operand(1);

        if (src->val().type().width() == 32)
            src = builder().insert_bitcast(value_type::f32(), src->val());
        else if (src->val().type().width() == 64)
            src = builder().insert_bitcast(value_type::f64(), src->val());

        if (src->val().type().width() != 80) {
            src = builder().insert_convert(dst->val().type(), src->val());
        }

        auto res = builder().insert_div(dst->val(), src->val());
        write_operand(0, res->val());

        // TODO FPU flags
        break;
    }
    case XED_ICLASS_FPREM: {
        // xed encoding: fprem st(0) st(1)
        auto st0 = read_operand(0);
        auto st1 = read_operand(1);

        auto res = builder().insert_mod(st0->val(), st1->val());

        write_operand(0, res->val());

        // TODO FPU flags
        break;
    }
    case XED_ICLASS_FLDZ: {
        auto zero = builder().insert_constant_f(
            value_type(value_type_class::floating_point, 80), +0.0);
        fpu_stack_top_move(-1);
        fpu_stack_set(0, zero->val());

        // TODO FPU flags
        break;
    }
    case XED_ICLASS_FLD1: {
        auto one = builder().insert_constant_f(
            value_type(value_type_class::floating_point, 80), +1.0);
        fpu_stack_top_move(-1);
        fpu_stack_set(0, one->val());

        // TODO FPU flags
        break;
    }
    case XED_ICLASS_FCHS: {
        // xed encoding: fchs st(0)
        auto st0 = read_operand(0);
        write_operand(0, builder().insert_not(st0->val())->val());

        // TODO flag management
        break;
    }
    case XED_ICLASS_FABS: {
        auto st0 = read_operand(0);
        st0 = builder().insert_bit_insert(
            st0->val(), builder().insert_constant_u1(1)->val(), 79, 1);
        write_operand(0, st0->val());

        // TODO set C1 flag to 0
        break;
    }
    case XED_ICLASS_FXCH: {
        // xed encoding: fxch st(i) st(j)
        auto sti = read_operand(0);
        auto stj = read_operand(1);
        auto tmp = builder().alloc_local(sti->val().type());
        builder().insert_write_local(tmp, sti->val());

        write_operand(0, stj->val());
        write_operand(1, builder().insert_read_local(tmp)->val());

        // TODO set C1 flag to 0
        break;
    }
    case XED_ICLASS_FCOMI:
    case XED_ICLASS_FCOMIP:
    case XED_ICLASS_FUCOMI:
    case XED_ICLASS_FUCOMIP: {
        // xed encoding: fucomi(p) st(0), st(i)
        // TODO properly manage the unordered case (SNan, QNaN, etc), and diff
        // with F* and FU*

        // get stack top and operand
        auto st0 = read_operand(0);
        auto sti = read_operand(1);

        // comparisons
        st0 = builder().insert_convert(value_type::f64(), st0->val());
        sti = builder().insert_convert(value_type::f64(), sti->val());

        auto cmplt = builder().insert_binop(binary_arith_op::cmpolt, st0->val(),
                                            sti->val());
        auto cmpeq = builder().insert_binop(binary_arith_op::cmpoeq, sti->val(),
                                            st0->val());

        auto zf = builder().insert_csel(
            cmpeq->val(),
            builder().insert_constant_i(value_type::u1(), 1)->val(),
            builder().insert_constant_i(value_type::u1(), 0)->val());
        auto cf = builder().insert_csel(
            cmplt->val(),
            builder().insert_constant_i(value_type::u1(), 1)->val(),
            builder().insert_constant_i(value_type::u1(), 0)->val());
        auto pf = builder().insert_constant_i(value_type::u1(), 0);

        builder().insert_write_reg(util::to_underlying(reg_offsets::ZF),
                                   util::to_underlying(reg_idx::ZF), "ZF",
                                   zf->val());
        builder().insert_write_reg(util::to_underlying(reg_offsets::CF),
                                   util::to_underlying(reg_idx::CF), "CF",
                                   cf->val());
        builder().insert_write_reg(util::to_underlying(reg_offsets::PF),
                                   util::to_underlying(reg_idx::PF), "PF",
                                   pf->val());

        break;
    }
    default:
        throw std::runtime_error(
            std::string("unsupported fpu operation") +
            xed_iclass_enum_t2str(xed_decoded_inst_get_iclass(xed_inst())));
    }

    switch (inst_class) {
    case XED_ICLASS_FSTP:
    case XED_ICLASS_FISTP:
    case XED_ICLASS_FADDP:
    case XED_ICLASS_FSUBP:
    case XED_ICLASS_FSUBRP:
    case XED_ICLASS_FMULP:
    case XED_ICLASS_FDIVP:
    case XED_ICLASS_FDIVRP:
    case XED_ICLASS_FCOMIP:
    case XED_ICLASS_FUCOMIP:
        fpu_stack_top_move(1);
        break;
    default:
        break;
    }
}
