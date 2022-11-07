#include <iostream>

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

	switch (xed_decoded_inst_get_iclass(insn)) {
	case XED_ICLASS_MUL: {
		/* mul %reg is decoded as mul %reg %rax %rdx */
		auto ax = pkt()->insert_zx(value_type(value_type_class::unsigned_integer, op[1]->val().type().element_width() * 2,
								   op[1]->val().type().nr_elements()), op[1]->val());
		auto castop = pkt()->insert_zx(ax->val().type(), op[0]->val());
		auto rslt = pkt()->insert_mul(ax->val(), castop->val());
		if (op[0]->val().type().width() == 8) {
			write_reg(reg_to_offset(XED_REG_AX), rslt->val());
		} else {
			auto low = pkt()->insert_bit_extract(rslt->val(), 0, op[0]->val().type().width());
			auto high = pkt()->insert_bit_extract(rslt->val(), op[0]->val().type().width() - 1, op[0]->val().type().width());

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
				std::cerr << "imul ";
				for (auto o : op) {
					std::cerr << o->val().type().to_string() << " ";
				}
				std::cerr << std::endl;

				auto ax = builder().insert_bitcast(op[1]->val().type().get_signed_type(), op[1]->val());
				auto castop = builder().insert_bitcast(ax->val().type(), op[0]->val());
				ax = builder().insert_sx(
					value_type(value_type_class::signed_integer, ax->val().type().element_width() * 2, ax->val().type().nr_elements()), ax->val());
				castop = builder().insert_sx(ax->val().type(), castop->val());
				rslt = builder().insert_mul(ax->val(), castop->val());
				if (op[0]->val().type().width() == 8) {
					write_reg(reg_to_offset(XED_REG_AX), rslt->val());
				} else {
					auto low = builder().insert_bit_extract(rslt->val(), 0, op[0]->val().type().width());
					auto high = builder().insert_bit_extract(rslt->val(), op[0]->val().type().width() - 1, op[0]->val().type().width());

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

	case XED_ICLASS_DIV:
	case XED_ICLASS_IDIV: {
		// std::cerr << "DIV operand types[" << std::to_string(xed_decoded_inst_noperands(insn)) << "]: op0:" << op0->val().type().to_string() << "; op1: " <<
		// op1->val().type().to_string() << "; op2: " << op2->val().type().to_string()  << std::endl;
		auto rslt = builder().insert_div(op[0]->val(), op[1]->val());

		write_operand(0, rslt->val());
		write_flags(rslt, flag_op::ignore, flag_op::set0, flag_op::set0, flag_op::ignore, flag_op::ignore, flag_op::ignore);
		break;
	}

	default:
		throw std::runtime_error("unsupported mul/div operation");
	}
}
