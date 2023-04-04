#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void muldiv_translator::do_translate()
{
	xed_decoded_inst_t *insn = xed_inst();
	auto nops = xed_decoded_inst_noperands(insn);
	arancini::ir::value_node *op[nops - 1];

	for (unsigned int i = 0; i < nops - 1; i++) {
		op[i] = read_operand(i);
	}

	auto inst = xed_decoded_inst_get_iclass(insn);
  switch (inst) {
	case XED_ICLASS_MUL: {
		/* mul %reg is decoded as mul %reg %rax %rdx */
		auto ax = builder().insert_zx(value_type(value_type_class::unsigned_integer, op[1]->val().type().element_width() * 2,
								   op[1]->val().type().nr_elements()), op[1]->val());
		auto castop = builder().insert_zx(ax->val().type(), op[0]->val());
		auto rslt = builder().insert_mul(ax->val(), castop->val());
		if (op[0]->val().type().width() == 8) {
			write_reg(xedreg_to_offset(XED_REG_AX), rslt->val());
		} else {
			auto low = builder().insert_bit_extract(rslt->val(), 0, op[0]->val().type().width());
			auto high = builder().insert_bit_extract(rslt->val(), op[0]->val().type().width(), op[0]->val().type().width());

			write_operand(1, low->val());
			write_operand(2, high->val());
		}
		write_flags(rslt, flag_op::ignore, flag_op::update, flag_op::update, flag_op::ignore, flag_op::ignore, flag_op::ignore);
		break;
	}
	case XED_ICLASS_IMUL: {
		arancini::ir::value_node *rslt;

		switch (nops - 1) {
		case 2: {
			/* 2 operands: op0 := op0 * op1 */
			auto op0_cast = builder().insert_bitcast(op[0]->val().type().get_signed_type(), op[0]->val());
			auto op1_cast = builder().insert_bitcast(op[1]->val().type().get_signed_type(), op[1]->val());
			auto op0_ext = builder().insert_sx(
				value_type(value_type_class::signed_integer, op0_cast->val().type().element_width() * 2, op0_cast->val().type().nr_elements()),
				op0_cast->val());
			auto op1_ext = builder().insert_sx(op0_ext->val().type(), op1_cast->val());
			rslt = builder().insert_mul(op0_ext->val(), op1_ext->val());
			auto trunc_rslt = builder().insert_trunc(op0_cast->val().type(), rslt->val());
			// cast back to unsigned since operand0 is seen as unsigned.
			// not sure if that's a good thing...
			trunc_rslt = builder().insert_bitcast(op[0]->val().type(), trunc_rslt->val());
			write_operand(0, trunc_rslt->val());
			break;
		}
		case 3: {
			if (op[2]->kind() != node_kinds::constant) {
				/*
				 * 1 operand: RDX:RAX := RAX * op0
				 * xed decodes this with op[1] = ax and op[2] = dx (and their variants)
				 */
				auto ax = builder().insert_bitcast(op[1]->val().type().get_signed_type(), op[1]->val());
				auto castop = builder().insert_bitcast(ax->val().type(), op[0]->val());
				ax = builder().insert_sx(
					value_type(value_type_class::signed_integer, ax->val().type().element_width() * 2, ax->val().type().nr_elements()), ax->val());
				castop = builder().insert_sx(ax->val().type(), castop->val());
				rslt = builder().insert_mul(ax->val(), castop->val());
				if (op[0]->val().type().width() == 8) {
					write_reg(xedreg_to_offset(XED_REG_AX), rslt->val());
				} else {
					auto low = builder().insert_bit_extract(rslt->val(), 0, op[0]->val().type().width());
					auto high = builder().insert_bit_extract(rslt->val(), op[0]->val().type().width(), op[0]->val().type().width());

					write_operand(1, low->val());
					write_operand(2, high->val());
				}
			} else {
				/* 3 operands: op0 := op1 * op2 */
				auto op1_cast = builder().insert_bitcast(op[1]->val().type().get_signed_type(), op[1]->val());
				auto op2_cast = builder().insert_bitcast(op[2]->val().type().get_signed_type(), op[2]->val());
				auto op1_ext = builder().insert_sx(
					value_type(value_type_class::signed_integer, op1_cast->val().type().element_width() * 2, op1_cast->val().type().nr_elements()),
					op1_cast->val());
				auto op2_ext = builder().insert_sx(op1_ext->val().type(), op2_cast->val());
				rslt = builder().insert_mul(op1_ext->val(), op2_ext->val());
				auto trunc_rslt = builder().insert_trunc(op[0]->val().type().get_signed_type(), rslt->val());
				trunc_rslt = builder().insert_bitcast(op[0]->val().type(), trunc_rslt->val());
				write_operand(0, trunc_rslt->val());
				break;
			}
			break;
		}
		default:
			throw std::runtime_error("unsupported number of operands for IMUL: " + std::to_string(nops - 1));
		}
		write_flags(rslt, flag_op::ignore, flag_op::update, flag_op::update, flag_op::ignore, flag_op::ignore, flag_op::ignore);
		break;
	}

	/*
	 * Both DIV and IDIV instructions do the same things, except that IDIV is signed and DIV is unsigned
	 * idiv %reg -> idiv %reg %rax %rdx
	 * %rdx:%rax = dividend, %reg = divisor
	 * rax := quotient, rdx := remainder
	 * except for the 8bit variant, where al := quotient, ah := remainder
	 * TODO: support the 8bit variant properly
	 */
	case XED_ICLASS_IDIV: {
		auto input_size = op[1]->val().type().element_width();

		auto dividend = builder().insert_zx(value_type(value_type_class::unsigned_integer, input_size * 2, 1), op[1]->val());
		dividend = builder().insert_bit_insert(dividend->val(), op[2]->val(), input_size, input_size);
		dividend = builder().insert_bitcast(dividend->val().type().get_signed_type(), dividend->val());

		auto divisor = builder().insert_bitcast(op[0]->val().type().get_signed_type(), op[0]->val());
		auto out_type = divisor->val().type();
		divisor = builder().insert_sx(dividend->val().type(), divisor->val());

		auto quo = builder().insert_div(dividend->val(), divisor->val());
		auto rem = builder().insert_mod(dividend->val(), divisor->val());

		quo = builder().insert_trunc(out_type, quo->val());
		rem = builder().insert_trunc(out_type, rem->val());

		write_operand(1, quo->val());
		write_operand(2, rem->val());
		break;
	}
	case XED_ICLASS_DIV: {
		auto input_size = op[1]->val().type().element_width();
		auto u_double_t = value_type(value_type_class::unsigned_integer, input_size * 2, 1);

		auto dividend = builder().insert_zx(u_double_t, op[1]->val());
		dividend = builder().insert_bit_insert(dividend->val(), op[2]->val(), input_size, input_size);

		auto divisor = builder().insert_zx(u_double_t, op[0]->val());

		auto quo = builder().insert_div(dividend->val(), divisor->val());
		auto rem = builder().insert_mod(dividend->val(), divisor->val());

		quo = builder().insert_trunc(op[1]->val().type(), quo->val());
		rem = builder().insert_trunc(op[1]->val().type(), rem->val());

		write_operand(1, quo->val());
		write_operand(2, rem->val());
		break;
	}

  case XED_ICLASS_MULSD: {
    auto dst = read_operand(0);
    auto src = read_operand(1);

    auto src_low = builder().insert_bit_extract(src->val(), 0, 64);
    auto dst_low = builder().insert_bit_extract(dst->val(), 0, 64);
    auto mul = builder().insert_mul(src_low->val(), dst_low->val());
    dst = builder().insert_bit_insert(dst->val(), mul->val(), 0, 64);
    write_operand(0, dst->val());
    break;
  }

  case XED_ICLASS_MULSS: {
    auto dst = read_operand(0);
    auto src = read_operand(1);

    if (src->val().type().width() != 32) {
      src = builder().insert_bit_extract(src->val(), 0, 32);
    }
    auto dst_low = builder().insert_bit_extract(dst->val(), 0, 32);

    auto mul = builder().insert_mul(dst_low->val(), src->val());
    dst = builder().insert_bit_insert(dst->val(), mul->val(), 0, 32);
    write_operand(0, dst->val());

    break;
  }

  case XED_ICLASS_DIVSS:
  case XED_ICLASS_DIVSD: {
    // divss xmm1, xmm2/m32
    auto dst = read_operand(0);
    auto src = read_operand(1);
    auto size = (inst == XED_ICLASS_DIVSS) ? 32 : 64;

    auto dst_low = builder().insert_bitcast(value_type(value_type_class::floating_point, size),
                                            builder().insert_bit_extract(dst->val(), 0, size)->val());
    if (src->val().type().width() == 128) { // if src is xmm
			src = builder().insert_bit_extract(src->val(), 0, size);
    }
    src = builder().insert_bitcast(value_type(value_type_class::floating_point, size), src->val());

    auto div = builder().insert_div(dst_low->val(), src->val());
    dst = builder().insert_bit_insert(dst->val(), div->val(), 0, size);
    write_operand(0, dst->val());
    break;
  }

  case XED_ICLASS_PMULUDQ: {
    // pmuludq xmm1, xmm2/m128 or pmuludq mm1, mm2/m64
    auto dst = read_operand(0);
    auto src = read_operand(1);

    if (dst->val().type().width() == 64) {
      auto dst_low = builder().insert_zx(value_type::u64(), builder().insert_bit_extract(dst->val(), 0, 32)->val());
      auto src_low = builder().insert_zx(value_type::u64(), builder().insert_bit_extract(src->val(), 0, 32)->val());
      auto mul = builder().insert_mul(dst_low->val(), src_low->val());
      write_operand(0, mul->val());
    } else {
      auto dst_vec = builder().insert_bitcast(value_type::vector(value_type::u32(), 4), dst->val());
      auto src_vec = builder().insert_bitcast(value_type::vector(value_type::u32(), 4), src->val());
      auto mul0 = builder().insert_mul(builder().insert_zx(value_type::u64(), builder().insert_vector_extract(dst_vec->val(), 0)->val())->val(),
                                       builder().insert_zx(value_type::u64(), builder().insert_vector_extract(src_vec->val(), 0)->val())->val());
      auto mul1 = builder().insert_mul(builder().insert_zx(value_type::u64(), builder().insert_vector_extract(dst_vec->val(), 2)->val())->val(),
                                       builder().insert_zx(value_type::u64(), builder().insert_vector_extract(src_vec->val(), 2)->val())->val());
      dst = builder().insert_bit_insert(dst->val(), mul0->val(), 0, 64);
      dst = builder().insert_bit_insert(dst->val(), mul1->val(), 64, 64);
      write_operand(0, dst->val());
    }
    break;
  }

  case XED_ICLASS_PMULLW: {
    auto dst = read_operand(0);
    auto src = read_operand(1);
    auto nr_elt = dst->val().type().width() / 16;

    dst = builder().insert_bitcast(value_type::vector(value_type::s16(), nr_elt), dst->val());
    src = builder().insert_bitcast(value_type::vector(value_type::s16(), nr_elt), src->val());
    for (int i = 0; i < nr_elt; i++) {
      auto dst_elt = builder().insert_sx(value_type::s32(), builder().insert_vector_extract(dst->val(), i)->val());
      auto src_elt = builder().insert_sx(value_type::s32(), builder().insert_vector_extract(src->val(), i)->val());
      auto mul = builder().insert_mul(dst_elt->val(), src_elt->val());
      auto res_elt = builder().insert_bit_extract(mul->val(), 0, 16);
      dst = builder().insert_vector_insert(dst->val(), i, res_elt->val());
    }
    write_operand(0, dst->val());
    break;
  }

	default:
		throw std::runtime_error("unsupported mul/div operation");
	}
}
