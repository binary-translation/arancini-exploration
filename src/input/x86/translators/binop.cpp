#include <iostream>
#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void binop_translator::do_translate()
{
	auto op0 = read_operand(0);
	value_node *rslt;

	value_type ty = op0->val().type();
	std::cerr << "bits width: " << ty.width() << "\n";

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_PADDB:
	case XED_ICLASS_PSUBB:
          ty = value_type::vector(value_type::u8(), ty.width()/8);
	  op0 = auto_cast(ty, op0);
	  break;
	case XED_ICLASS_PADDW:
	case XED_ICLASS_PSUBW:
          ty = value_type::vector(value_type::u16(), ty.width()/16);
	  op0 = auto_cast(ty, op0);
	  break;
	case XED_ICLASS_PADDD:
	case XED_ICLASS_PSUBD:
          ty = value_type::vector(value_type::u32(), ty.width()/32);
	  op0 = auto_cast(ty, op0);
	  break;
	case XED_ICLASS_PADDQ:
	case XED_ICLASS_PSUBQ:
          ty = value_type::vector(value_type::u64(), ty.width()/64);
	  op0 = auto_cast(ty, op0);
	  break;
	default:
	  break;
	}

	std::cerr << "bits width: " << ty.width() << "\n";
	std::cerr << "is vector: " << ty.is_vector() << "\n";
	std::cerr << "elements width: " << ty.element_width() << "\n";
	std::cerr << "number elements: " << ty.number_elements() << "\n";

	auto op1 = auto_cast(ty, read_operand(1));

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_XOR:
	case XED_ICLASS_PXOR:
		rslt = pkt()->insert_xor(op0->val(), op1->val());
		break;
	case XED_ICLASS_AND:
	case XED_ICLASS_PAND:
	case XED_ICLASS_TEST:
		rslt = pkt()->insert_and(op0->val(), op1->val());
		break;
	case XED_ICLASS_OR:
	case XED_ICLASS_POR:
		rslt = pkt()->insert_or(op0->val(), op1->val());
		break;

	case XED_ICLASS_ADD:
	case XED_ICLASS_PADDB:
	case XED_ICLASS_PADDW:
	case XED_ICLASS_PADDD:
	case XED_ICLASS_PADDQ:
		rslt = pkt()->insert_add(op0->val(), op1->val());
		break;
	case XED_ICLASS_ADC:
		rslt = pkt()->insert_adc(op0->val(), op1->val(), auto_cast(op0->val().type(), read_reg(value_type::u1(), reg_offsets::cf))->val());
		break;
	case XED_ICLASS_SUB:
	case XED_ICLASS_PSUBB:
	case XED_ICLASS_PSUBW:
	case XED_ICLASS_PSUBD:
	case XED_ICLASS_PSUBQ:
	case XED_ICLASS_CMP:
		rslt = pkt()->insert_sub(op0->val(), op1->val());
		break;
	case XED_ICLASS_SBB:
		rslt = pkt()->insert_sbb(op0->val(), op1->val(), auto_cast(op0->val().type(), read_reg(value_type::u1(), reg_offsets::cf))->val());
		break;

	default:
		throw std::runtime_error("unsupported binop");
	}

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_CMP:
	case XED_ICLASS_TEST:
		break;

	default:
		write_operand(0, rslt->val());
		break;
	}

	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_XOR:
	case XED_ICLASS_AND:
	case XED_ICLASS_TEST:
	case XED_ICLASS_OR:
		write_flags(rslt, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::ignore);
		break;

	case XED_ICLASS_ADD:
	case XED_ICLASS_ADC:
	case XED_ICLASS_SUB:
	case XED_ICLASS_SBB:
	case XED_ICLASS_CMP:
		write_flags(rslt, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update);
		break;

	default:
		break;
	}
}
