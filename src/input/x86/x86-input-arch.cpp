#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/ir/builder.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/port.h>
#include <arancini/ir/value-type.h>
#include <iostream>
#include <sstream>

extern "C" {
#include <xed/xed-interface.h>
}

using namespace arancini::ir;
using namespace arancini::input::x86;

static void initialise_xed()
{
	static bool has_initialised_xed = false;

	if (!has_initialised_xed) {
		xed_tables_init();
		has_initialised_xed = true;
	}
}

static int reg_to_offset(xed_reg_enum_t reg) { return (xed_get_largest_enclosing_register(reg) - XED_REG_RAX); }

static value_node *read_operand(packet *pkt, xed_decoded_inst_t *xed_inst, int opnum)
{
	const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst);
	auto operand = xed_inst_operand(insn, opnum);
	auto opname = xed_operand_name(operand);

	switch (opname) {
	case XED_OPERAND_REG0:
	case XED_OPERAND_REG1: {
		auto reg = xed_decoded_inst_get_reg(xed_inst, opname);
		auto regclass = xed_reg_class(reg);

		switch (regclass) {
		case XED_REG_CLASS_GPR:
			return pkt->insert_read_reg(value_type::u32(), reg_to_offset(reg));

		default:
			throw std::runtime_error("unsupported register");
		}
	}

	case XED_OPERAND_IMM0: {
		switch (xed_decoded_inst_get_immediate_width_bits(xed_inst)) {
		case 8:
			return pkt->insert_constant_u64(xed_decoded_inst_get_unsigned_immediate(xed_inst));

		case 16:
			return pkt->insert_constant_u64(xed_decoded_inst_get_unsigned_immediate(xed_inst));

		case 32:
			return pkt->insert_constant_u64(xed_decoded_inst_get_unsigned_immediate(xed_inst));

		case 64:
			return pkt->insert_constant_u64(xed_decoded_inst_get_unsigned_immediate(xed_inst));

		default:
			throw std::runtime_error("unsupported immediate width");
		}
	}

	default:
		throw std::logic_error("unsupported read operand type");
	}
}

static action_node *write_operand(packet *pkt, xed_decoded_inst_t *xed_inst, int opnum, const port &value)
{
	const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst);
	auto operand = xed_inst_operand(insn, opnum);
	auto opname = xed_operand_name(operand);

	switch (opname) {
	case XED_OPERAND_REG0:
	case XED_OPERAND_REG1: {
		auto reg = xed_decoded_inst_get_reg(xed_inst, opname);
		auto regclass = xed_reg_class(reg);

		switch (regclass) {
		case XED_REG_CLASS_GPR:
			return pkt->insert_write_reg(reg_to_offset(reg), value);

		default:
			throw std::runtime_error("unsupported register");
		}
	}

	default:
		throw std::logic_error("unsupported write operand type");
	}
}

enum flag_op { ignore, set0, set1, update };

static void write_flags(packet *pkt, xed_decoded_inst_t *xed_inst, binary_arith_node *op, flag_op zf, flag_op cf, flag_op of, flag_op sf, flag_op pf)
{
	switch (zf) {
	case flag_op::set0:
		pkt->insert_write_reg(100, pkt->insert_constant(value_type::u1(), 0)->val());
		break;
	case flag_op::set1:
		pkt->insert_write_reg(100, pkt->insert_constant(value_type::u1(), 1)->val());
		break;
	case flag_op::update:
		pkt->insert_write_reg(100, op->zero());
		break;
	default:
		break;
	}

	switch (cf) {
	case flag_op::set0:
		pkt->insert_write_reg(101, pkt->insert_constant(value_type::u1(), 0)->val());
		break;
	case flag_op::set1:
		pkt->insert_write_reg(101, pkt->insert_constant(value_type::u1(), 1)->val());
		break;
	case flag_op::update:
		pkt->insert_write_reg(101, op->carry());
		break;
	default:
		break;
	}

	switch (of) {
	case flag_op::set0:
		pkt->insert_write_reg(102, pkt->insert_constant(value_type::u1(), 0)->val());
		break;
	case flag_op::set1:
		pkt->insert_write_reg(102, pkt->insert_constant(value_type::u1(), 1)->val());
		break;
	case flag_op::update:
		pkt->insert_write_reg(102, op->overflow());
		break;
	default:
		break;
	}

	switch (sf) {
	case flag_op::set0:
		pkt->insert_write_reg(103, pkt->insert_constant(value_type::u1(), 0)->val());
		break;
	case flag_op::set1:
		pkt->insert_write_reg(103, pkt->insert_constant(value_type::u1(), 1)->val());
		break;
	case flag_op::update:
		pkt->insert_write_reg(103, op->negative());
		break;
	default:
		break;
	}
}

static void translate_instruction(builder &b, off_t address, xed_decoded_inst_t *xed_inst)
{
	auto pkt = b.insert_packet();
	pkt->insert_start(address);

	char buffer[64];
	// xed_decoded_inst_dump(xed_inst, buffer, sizeof(buffer));
	xed_format_context(XED_SYNTAX_ATT, xed_inst, buffer, sizeof(buffer), address, nullptr, 0);

	std::cerr << "insn @ " << std::hex << address << ": " << buffer << std::endl;

	switch (xed_decoded_inst_get_iclass(xed_inst)) {
	case XED_ICLASS_XOR: {
		auto op0 = read_operand(pkt, xed_inst, 0);
		auto op1 = read_operand(pkt, xed_inst, 1);
		auto rslt = pkt->insert_xor(op0->val(), op1->val());

		write_operand(pkt, xed_inst, 0, rslt->val());
		write_flags(pkt, xed_inst, rslt, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update);
		break;
	}

	case XED_ICLASS_AND: {
		auto op0 = read_operand(pkt, xed_inst, 0);
		auto op1 = read_operand(pkt, xed_inst, 1);
		auto rslt = pkt->insert_and(op0->val(), op1->val());

		write_operand(pkt, xed_inst, 0, rslt->val());
		write_flags(pkt, xed_inst, rslt, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update);
		break;
	}

	case XED_ICLASS_MOV: {
		auto op1 = read_operand(pkt, xed_inst, 1);
		write_operand(pkt, xed_inst, 0, op1->val());
		break;
	}

	case XED_ICLASS_PUSH: {
		auto rsp = pkt->insert_read_reg(value_type::u64(), reg_to_offset(XED_REG_RSP));
		auto new_rsp = pkt->insert_sub(rsp->val(), pkt->insert_constant_u64(8)->val());

		pkt->insert_write_reg(reg_to_offset(XED_REG_RSP), new_rsp->val());
		pkt->insert_write_mem(new_rsp->val(), read_operand(pkt, xed_inst, 0)->val());

		break;
	}

	case XED_ICLASS_POP: {
		auto rsp = pkt->insert_read_reg(value_type::u64(), reg_to_offset(XED_REG_RSP));
		write_operand(pkt, xed_inst, 0, pkt->insert_read_mem(value_type::u64(), rsp->val())->val());

		auto new_rsp = pkt->insert_add(rsp->val(), pkt->insert_constant_u64(8)->val());
		pkt->insert_write_reg(reg_to_offset(XED_REG_RSP), new_rsp->val());

		break;
	}

	case XED_ICLASS_CALL_FAR:
	case XED_ICLASS_CALL_NEAR: {
		// Push RIP+X to stack

		auto rsp = pkt->insert_read_reg(value_type::u64(), reg_to_offset(XED_REG_RSP));
		auto new_rsp = pkt->insert_sub(rsp->val(), pkt->insert_constant_u64(8)->val());

		pkt->insert_write_reg(reg_to_offset(XED_REG_RSP), new_rsp->val());

		xed_uint_t instruction_length = xed_decoded_inst_get_length(xed_inst);
		auto next_target_node = pkt->insert_add(pkt->insert_read_pc()->val(), pkt->insert_constant_u64(instruction_length)->val());
		pkt->insert_write_mem(new_rsp->val(), next_target_node->val());

		// Set PC to BLAH
		int32_t value = xed_decoded_inst_get_branch_displacement(xed_inst);
		uint64_t target = value + instruction_length;

		auto target_node = pkt->insert_add(pkt->insert_read_pc()->val(), pkt->insert_constant_u64(target)->val());

		pkt->insert_write_pc(target_node->val());

		// TERMINATE
		break;
	}

	case XED_ICLASS_TEST: {
		auto op0 = read_operand(pkt, xed_inst, 0);
		auto op1 = read_operand(pkt, xed_inst, 1);
		auto rslt = pkt->insert_and(op0->val(), op1->val());

		write_flags(pkt, xed_inst, rslt, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update);
		break;
	}

	case XED_ICLASS_JNZ: {
		break;
	}

	case XED_ICLASS_NOP:
	case XED_ICLASS_HLT:
		break;

	default:
		throw new std::runtime_error("unsupported instruction");
	}

	pkt->insert_end();
}

void x86_input_arch::translate_code(builder &builder, off_t base_address, const void *code, size_t code_size)
{
	initialise_xed();

	const uint8_t *mc = (const uint8_t *)code;

	size_t offset = 0;
	do {
		xed_decoded_inst_t xedd;
		xed_decoded_inst_zero(&xedd);
		xed_decoded_inst_set_mode(&xedd, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b);
		xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_ALL);

		xed_error_enum_t xed_error = xed_decode(&xedd, &mc[offset], code_size - offset);
		if (xed_error != XED_ERROR_NONE) {
			throw std::runtime_error("unable to decode instruction: " + std::to_string(xed_error));
		}

		xed_uint_t length = xed_decoded_inst_get_length(&xedd);

		translate_instruction(builder, base_address, &xedd);

		offset += length;
		base_address += length;
	} while (offset < code_size);
}
