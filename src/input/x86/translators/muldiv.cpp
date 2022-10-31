#include <iostream>

#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>

#include <csignal>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void muldiv_translator::do_translate()
{
	xed_decoded_inst_t *insn = xed_inst();
	auto nops = xed_decoded_inst_noperands(insn);
	arancini::ir::value_node *op[nops - 1];

	for (auto i = 0; i < nops - 1; i++)
		op[i] = read_operand(i);

	switch (xed_decoded_inst_get_iclass(insn)) {
	// case XED_ICLASS_MUL:
	case XED_ICLASS_IMUL: {
		arancini::ir::value_node *rslt;

		switch (nops - 1) {
		case 1: {
			/* 1 operand: RDX:RAX := RAX * op0 */
			arancini::ir::value_node *a_reg;

			switch (op[0]->val().type().width()) {
			case 8:
				a_reg = read_reg(value_type::s8(), reg_to_offset(XED_REG_AL));
				break;
			case 16:
				a_reg = read_reg(value_type::s16(), reg_to_offset(XED_REG_AX));
				break;
			case 32:
				a_reg = read_reg(value_type::s32(), reg_to_offset(XED_REG_EAX));
				break;
			case 64:
				a_reg = read_reg(value_type::s64(), reg_to_offset(XED_REG_RAX));
				break;
			default:
				throw std::runtime_error("unknown operand type for IMUL");
			}
			auto castop = pkt()->insert_bitcast(a_reg->val().type(), op[0]->val());
			rslt = pkt()->insert_mul(a_reg->val(), castop->val());
			switch (op[0]->val().type().width()) {
			case 8: {
				write_reg(reg_to_offset(XED_REG_AX), rslt->val());
				break;
			}
			case 16: {
				auto low = pkt()->insert_bit_extract(rslt->val(), 0, 16);
				auto high = pkt()->insert_bit_extract(rslt->val(), 16, 16);

				write_reg(reg_to_offset(XED_REG_AX), low->val());
				write_reg(reg_to_offset(XED_REG_DX), high->val());
				break;
			}
			case 32: {
				auto low = pkt()->insert_bit_extract(rslt->val(), 0, 32);
				auto high = pkt()->insert_bit_extract(rslt->val(), 32, 32);

				write_reg(reg_to_offset(XED_REG_EAX), low->val());
				write_reg(reg_to_offset(XED_REG_EDX), high->val());
				break;
			}
			case 64: {
				auto low = pkt()->insert_bit_extract(rslt->val(), 0, 64);
				auto high = pkt()->insert_bit_extract(rslt->val(), 64, 64);

				write_reg(reg_to_offset(XED_REG_RAX), low->val());
				write_reg(reg_to_offset(XED_REG_RDX), high->val());
				break;
			}
			default:
				throw std::runtime_error("unknown operand type for IMUL");
			}
			break;
		}
		case 2: {
			/* 2 operands: op0 := op0 * op1 */
			auto op0_cast = pkt()->insert_bitcast(op[0]->val().type().get_signed_type(), op[0]->val());
			auto op1_cast = pkt()->insert_bitcast(op[1]->val().type().get_signed_type(), op[1]->val());
			rslt = pkt()->insert_mul(op0_cast->val(), op1_cast->val());
			auto trunc_rslt = pkt()->insert_trunc(op0_cast->val().type(), rslt->val());
			// cast back to unsigned since operand0 is seen as unsigned.
			// not sure if that's a good thing...
			auto cast_rslt = pkt()->insert_bitcast(op[0]->val().type(), trunc_rslt->val());
			write_operand(0, cast_rslt->val());
			break;
		}
		case 3: {
			/* 3 operands: op0 := op1 * op2 */
			auto op1_cast = pkt()->insert_bitcast(op[1]->val().type().get_signed_type(), op[1]->val());
			auto op2_cast = pkt()->insert_bitcast(op[2]->val().type().get_signed_type(), op[2]->val());
			auto op2_ext = pkt()->insert_sx(op1_cast->val().type(), op2_cast->val());
			rslt = pkt()->insert_mul(op1_cast->val(), op2_ext->val());
			auto trunc_rslt = pkt()->insert_trunc(op1_cast->val().type(), rslt->val());
			write_operand(0, trunc_rslt->val());
			break;
		}
		default:
			throw std::runtime_error("unsupported number of operands for IMUL");
		}
		write_flags(rslt, flag_op::ignore, flag_op::update, flag_op::update, flag_op::ignore, flag_op::ignore, flag_op::ignore);
		break;
	}

	case XED_ICLASS_DIV:
	case XED_ICLASS_IDIV: {
		// std::cerr << "DIV operand types[" << std::to_string(xed_decoded_inst_noperands(insn)) << "]: op0:" << op0->val().type().to_string() << "; op1: " << op1->val().type().to_string() << "; op2: " << op2->val().type().to_string()  << std::endl;
		auto rslt = pkt()->insert_div(op[0]->val(), op[1]->val());

		write_operand(0, rslt->val());
		write_flags(rslt, flag_op::ignore, flag_op::set0, flag_op::set0, flag_op::ignore, flag_op::ignore, flag_op::ignore);
		break;
	}

	default:
		throw std::runtime_error("unsupported mul/div operation");
	}
}
