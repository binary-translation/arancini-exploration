#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/port.h>

#include <csignal>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

translation_result translator::translate(off_t address, xed_decoded_inst_t *xed_inst, const std::string& disasm)
{
	switch (xed_decoded_inst_get_iclass(xed_inst)) {
	// TODO: this is a bad way of avoiding empty packets. Should be done by checking that the translator is a nop_translator, not hardcoded switch case
	case XED_ICLASS_NOP:
	case XED_ICLASS_HLT:
	case XED_ICLASS_CPUID:
	case XED_ICLASS_SYSCALL:
	case XED_ICLASS_PREFETCHNTA:
		return translation_result::noop;

	default:
		builder_.begin_packet(address, disasm);

		xed_inst_ = xed_inst;
		do_translate();

		return builder_.end_packet() == packet_type::end_of_block ? translation_result::end_of_block : translation_result::normal;
	}
}

action_node *translator::write_operand(int opnum, port &value)
{
	const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst());
	auto operand = xed_inst_operand(insn, opnum);
	auto opname = xed_operand_name(operand);

	switch (opname) {
	case XED_OPERAND_REG0:
	case XED_OPERAND_REG1:
	case XED_OPERAND_REG2: {
		auto reg = xed_decoded_inst_get_reg(xed_inst(), opname);
		auto regclass = xed_reg_class(reg);

		switch (regclass) {
		case XED_REG_CLASS_GPR: {
			auto width = value.type().width();
			switch (width) {
			// Behaviour is extracted from the Intel® 64 and IA-32 Architectures Software Developer’s Manual, Volume 1 Basic Architecture, Section 3.4.1.1
			case 64: // e.g. RAX
				// value is bitcast to u64 and written
				if (value.type().type_class() == value_type_class::signed_integer) {
					return write_reg(reg_to_offset(reg), builder_.insert_bitcast(value_type::u64(), value)->val());
				} else {
					return write_reg(reg_to_offset(reg), value);
				}
			case 32: // e.g. EAX
				// x86_64 requires that the high 32bit are zeroed when writing to 32bit version of registers
				return write_reg(reg_to_offset(reg), builder_.insert_zx(value_type::u64(), value)->val());
			case 16: { // e.g. AX
				// x86_64 requires that the upper bits [63..16] are untouched
				auto orig = read_reg(value_type::u64(), reg_to_offset(reg));
				auto res = builder_.insert_bit_insert(orig->val(), value, 0, 16);
				return write_reg(reg_to_offset(reg), res->val());
			}
			case 8: { // e.g. AL/AH
				// x86_64 requires that the upper bits [63..16/8] are untouched
				auto orig = read_reg(value_type::u64(), reg_to_offset(reg));
				value_node *res;
				if (reg >= XED_REG_AL && reg <= XED_REG_DIL) { // lower 8 bits
					res = builder_.insert_bit_insert(orig->val(), value, 0, 8);
				} else { // bits [15..8]
					res = builder_.insert_bit_insert(orig->val(), value, 8, 8);
				}
				return write_reg(reg_to_offset(reg), res->val());
			}
			default:
				throw std::runtime_error("" + std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": unsupported general purpose register size: " + std::to_string(width));
			}
		}

		case XED_REG_CLASS_XMM:
			return write_reg(reg_to_offset(reg), builder_.insert_zx(value_type::u128(), value)->val());

		default:
			throw std::runtime_error("" + std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": unsupported register class: " + std::to_string(regclass));
		}
	}

	case XED_OPERAND_MEM0: {
		auto address = compute_address(0);
		return builder_.insert_write_mem(address->val(), value);
	}

	default:
		throw std::logic_error("unsupported write operand type: " + std::to_string((int)opname));
	}
}

value_node *translator::read_operand(int opnum)
{
	const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst());
	auto operand = xed_inst_operand(insn, opnum);
	auto opname = xed_operand_name(operand);

	switch (opname) {
	case XED_OPERAND_REG0:
	case XED_OPERAND_REG1:
	case XED_OPERAND_REG2: {
		auto reg = xed_decoded_inst_get_reg(xed_inst(), opname);
		auto regclass = xed_reg_class(reg);

		switch (regclass) {
		case XED_REG_CLASS_GPR:
			switch (xed_get_register_width_bits(reg)) {
			case 8:
				return read_reg(value_type::u8(), reg_to_offset(reg));
			case 16:
				return read_reg(value_type::u16(), reg_to_offset(reg));
			case 32:
				return read_reg(value_type::u32(), reg_to_offset(reg));
			case 64:
				return read_reg(value_type::u64(), reg_to_offset(reg));
			default:
				throw std::runtime_error("unsupported register size");
			}

		case XED_REG_CLASS_XMM:
			return read_reg(value_type::u128(), reg_to_offset(reg));

		case XED_REG_CLASS_FLAGS:
			return read_reg(value_type::u64(), reg_to_offset(reg));

		default:
			throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": unsupported register class: " + std::to_string(regclass));
		}
	}

	case XED_OPERAND_IMM0: {
		switch (xed_decoded_inst_get_immediate_width_bits(xed_inst())) {
		case 8:
			return builder_.insert_constant_s32(xed_decoded_inst_get_signed_immediate(xed_inst()));

		case 16:
			return builder_.insert_constant_s32(xed_decoded_inst_get_signed_immediate(xed_inst()));

		case 32:
			return builder_.insert_constant_s32(xed_decoded_inst_get_signed_immediate(xed_inst()));

		case 64:
			return builder_.insert_constant_s32(xed_decoded_inst_get_signed_immediate(xed_inst()));

		default:
			throw std::runtime_error("unsupported immediate width");
		}
	}

	case XED_OPERAND_MEM0: {
		auto addr = compute_address(0);

		switch (xed_decoded_inst_get_memory_operand_length(xed_inst(), 0)) {
		case 1:
			return builder_.insert_read_mem(value_type::u8(), addr->val());
		case 2:
			return builder_.insert_read_mem(value_type::u16(), addr->val());
		case 4:
			return builder_.insert_read_mem(value_type::u32(), addr->val());
		case 8:
			return builder_.insert_read_mem(value_type::u64(), addr->val());
		case 16:
			return builder_.insert_read_mem(value_type::u128(), addr->val());

		default:
			throw std::runtime_error("invalid memory width in read");
		}
	}

	default:
		throw std::logic_error("unsupported read operand type: " + std::to_string((int)opname));
	}
}

ssize_t translator::get_operand_width(int opnum)
{
	const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst());
	auto operand = xed_inst_operand(insn, opnum);
	auto opname = xed_operand_name(operand);

	switch (opname) {
	case XED_OPERAND_REG0:
	case XED_OPERAND_REG1:
	case XED_OPERAND_REG2: {
		auto reg = xed_decoded_inst_get_reg(xed_inst(), opname);
		return xed_get_register_width_bits(reg);
	}
	case XED_OPERAND_IMM0: {
		return xed_decoded_inst_get_immediate_width_bits(xed_inst());
	}
	case XED_OPERAND_MEM0: {
		return xed_decoded_inst_get_memory_operand_length(xed_inst(), 0);
	}
	default:
		throw std::runtime_error("unsupported operand width query");
	}
}

bool translator::is_memory_operand(int opnum)
{
	const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst());
	auto operand = xed_inst_operand(insn, opnum);
	auto opname = xed_operand_name(operand);

	return (opname == XED_OPERAND_MEM0);
}

value_node *translator::compute_address(int mem_idx)
{
	auto base_reg = xed_decoded_inst_get_base_reg(xed_inst(), mem_idx);
	auto displ = xed_decoded_inst_get_memory_displacement(xed_inst(), mem_idx);

	auto index = xed_decoded_inst_get_index_reg(xed_inst(), mem_idx);
	auto scale = xed_decoded_inst_get_scale(xed_inst(), mem_idx);

	auto seg = xed_decoded_inst_get_seg_reg(xed_inst(), mem_idx);

	if (xed_get_register_width_bits(base_reg) != 64 && base_reg != XED_REG_INVALID) {
		throw std::runtime_error("base reg invalid size");
	}

	value_node *address_base;

	if (base_reg == XED_REG_INVALID) {
		address_base = builder_.insert_constant_u64(0);
	} else if (base_reg == XED_REG_RIP) {
		address_base = builder_.insert_read_pc();
	} else {
		address_base = read_reg(value_type::u64(), reg_to_offset(base_reg));
	}

	if (index != XED_REG_INVALID) {
		auto scaled_index = builder_.insert_mul(read_reg(value_type::u64(), reg_to_offset(index))->val(), builder_.insert_constant_u64(scale)->val());

		address_base = builder_.insert_add(address_base->val(), scaled_index->val());
	}

	if (displ) {
		address_base = builder_.insert_add(address_base->val(), builder_.insert_constant_u64(displ)->val());
	}

	if (seg == XED_REG_FS) {
		address_base = builder_.insert_add(address_base->val(), read_reg(value_type::u64(), reg_offsets::FS)->val());
	} else if (seg == XED_REG_GS) {
		address_base = builder_.insert_add(address_base->val(), read_reg(value_type::u64(), reg_offsets::GS)->val());
	}

	return address_base;
}

translator::reg_offsets translator::reg_to_offset(xed_reg_enum_t reg)
{
	switch (xed_reg_class(reg)) {
	case XED_REG_CLASS_GPR:
		return (reg_offsets)((((xed_get_largest_enclosing_register(reg) - XED_REG_RAX) + (int)reg_offsets::RAX)) * 8);

	case XED_REG_CLASS_XMM:
		return (reg_offsets)(((reg - XED_REG_XMM0) + (int)reg_offsets::XMM0) * 16);

	default:
		throw std::runtime_error("unsupported register class when computing offset");
	}
}

action_node *translator::write_reg(reg_offsets reg, port &value) { return builder_.insert_write_reg((unsigned long)reg, value); }

value_node *translator::read_reg(const value_type &vt, reg_offsets reg) { return builder_.insert_read_reg(vt, (unsigned long)reg); }

void translator::write_flags(value_node *op, flag_op zf, flag_op cf, flag_op of, flag_op sf, flag_op pf, flag_op af)
{
	switch (zf) {
	case flag_op::set0:
		write_reg(reg_offsets::ZF, builder_.insert_constant_i(value_type::u1(), 0)->val());
		break;

	case flag_op::set1:
		write_reg(reg_offsets::ZF, builder_.insert_constant_i(value_type::u1(), 1)->val());
		break;

	case flag_op::update:
		switch (op->kind()) {
		case node_kinds::unary_arith:
		case node_kinds::binary_arith:
		case node_kinds::ternary_arith:
			write_reg(reg_offsets::ZF, ((arith_node *)op)->zero());
			break;
		case node_kinds::unary_atomic:
		case node_kinds::binary_atomic:
		case node_kinds::ternary_atomic:
			write_reg(reg_offsets::ZF, ((atomic_node *)op)->zero());
			break;
		case node_kinds::constant:
			write_reg(reg_offsets::ZF, builder_.insert_constant_i(value_type::u1(), ((constant_node *)op)->is_zero())->val());
			break;
		default:
			throw std::runtime_error("unsupported operation node type for ZF");
		}
		break;

	default:
		break;
	}

	switch (cf) {
	case flag_op::set0:
		write_reg(reg_offsets::CF, builder_.insert_constant_i(value_type::u1(), 0)->val());
		break;

	case flag_op::set1:
		write_reg(reg_offsets::CF, builder_.insert_constant_i(value_type::u1(), 1)->val());
		break;

	case flag_op::update:
		switch (op->kind()) {
		case node_kinds::binary_arith:
		case node_kinds::ternary_arith:
			write_reg(reg_offsets::CF, ((arith_node *)op)->carry());
			break;
		case node_kinds::binary_atomic:
		case node_kinds::ternary_atomic:
			write_reg(reg_offsets::CF, ((atomic_node *)op)->carry());
			break;
		default:
			throw std::runtime_error("unsupported operation node type for CF");
		}
		break;

	default:
		break;
	}

	switch (of) {
	case flag_op::set0:
		write_reg(reg_offsets::OF, builder_.insert_constant_i(value_type::u1(), 0)->val());
		break;

	case flag_op::set1:
		write_reg(reg_offsets::OF, builder_.insert_constant_i(value_type::u1(), 1)->val());
		break;

	case flag_op::update:
		switch (op->kind()) {
		case node_kinds::binary_arith:
		case node_kinds::ternary_arith:
			write_reg(reg_offsets::OF, ((arith_node *)op)->overflow());
			break;
		case node_kinds::binary_atomic:
		case node_kinds::ternary_atomic:
			write_reg(reg_offsets::OF, ((atomic_node *)op)->overflow());
			break;
		default:
			throw std::runtime_error("unsupported operation node type for OF");
		}
		break;

	default:
		break;
	}

	switch (sf) {
	case flag_op::set0:
		write_reg(reg_offsets::SF, builder_.insert_constant_i(value_type::u1(), 0)->val());
		break;

	case flag_op::set1:
		write_reg(reg_offsets::SF, builder_.insert_constant_i(value_type::u1(), 1)->val());
		break;

	case flag_op::update:
		switch (op->kind()) {
		case node_kinds::unary_arith:
		case node_kinds::binary_arith:
		case node_kinds::ternary_arith:
			write_reg(reg_offsets::SF, ((arith_node *)op)->negative());
			break;
		case node_kinds::unary_atomic:
		case node_kinds::binary_atomic:
		case node_kinds::ternary_atomic:
			write_reg(reg_offsets::SF, ((atomic_node *)op)->negative());
			break;
		case node_kinds::constant:
			write_reg(reg_offsets::SF, builder_.insert_constant_i(value_type::u1(), ((constant_node *)op)->const_val_i() < 0)->val());
			break;
		default:
			throw std::runtime_error("unsupported operation node type for SF");
		}
		break;

	default:
		break;
	}
}

value_node *translator::compute_cond(cond_type ct)
{
	switch (ct) {
	case cond_type::nbe: {
		auto ncf = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::CF)->val());
		auto nzf = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::ZF)->val());
		return builder_.insert_and(ncf->val(), nzf->val());
	}

	case cond_type::nb: {
		auto ncf = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::CF)->val());
		return ncf;
	}

	case cond_type::b: {
		auto cf = read_reg(value_type::u1(), reg_offsets::CF);
		return cf;
	}

	case cond_type::be: {
		auto cf = read_reg(value_type::u1(), reg_offsets::CF);
		auto zf = read_reg(value_type::u1(), reg_offsets::ZF);
		return builder_.insert_or(cf->val(), zf->val());
	}

	case cond_type::z: {
		auto zf = read_reg(value_type::u1(), reg_offsets::ZF);
		return zf;
	}

	case cond_type::nle: {
		auto nzf = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::ZF)->val());
		auto sf = read_reg(value_type::u1(), reg_offsets::SF);
		auto of = read_reg(value_type::u1(), reg_offsets::OF);

		return builder_.insert_and(nzf->val(), builder_.insert_cmpeq(sf->val(), of->val())->val());
	}

	case cond_type::nl: {
		auto sf = read_reg(value_type::u1(), reg_offsets::SF);
		auto of = read_reg(value_type::u1(), reg_offsets::OF);

		return builder_.insert_cmpeq(sf->val(), of->val());
	}

	case cond_type::l: {
		auto sf = read_reg(value_type::u1(), reg_offsets::SF);
		auto of = read_reg(value_type::u1(), reg_offsets::OF);

		return builder_.insert_cmpne(sf->val(), of->val());
	}

	case cond_type::le: {
		auto zf = read_reg(value_type::u1(), reg_offsets::ZF);
		auto sf = read_reg(value_type::u1(), reg_offsets::SF);
		auto of = read_reg(value_type::u1(), reg_offsets::OF);

		return builder_.insert_or(zf->val(), builder_.insert_cmpne(sf->val(), of->val())->val());
	}

	case cond_type::nz: {
		auto nzf = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::ZF)->val());
		return nzf;
	}

	case cond_type::no: {
		auto nof = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::OF)->val());
		return nof;
	}

	case cond_type::np: {
		auto npf = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::PF)->val());
		return npf;
	}

	case cond_type::ns: {
		auto nsf = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::SF)->val());
		return nsf;
	}

	case cond_type::o: {
		auto of = read_reg(value_type::u1(), reg_offsets::OF);
		return of;
	}

	case cond_type::p: {
		auto pf = read_reg(value_type::u1(), reg_offsets::PF);
		return pf;
	}

	case cond_type::s: {
		auto sf = read_reg(value_type::u1(), reg_offsets::SF);
		return sf;
	}

	default:
		throw std::runtime_error("unhandled condition code");
	}
}

value_node *translator::auto_cast(const value_type &target_type, value_node *v)
{
	const auto &vtype = v->val().type();

	// If the widths of the type are the same, then we might only have to do a bitcast.
	if (target_type.width() == vtype.width()) {
		if (target_type.type_class() != vtype.type_class()) {
			// If the type classes are different, then just do a bitcast.
			return builder_.insert_bitcast(target_type, v->val());
		} else {
			// Otherwise, there's actually nothing to do.
			return v;
		}
	}

	// If we get here, we know we need to do a truncation, or an extension.
	value_node *r;

	if (target_type.width() < vtype.width()) {
		// This is a truncation
		r = builder_.insert_trunc(value_type(vtype.type_class(), target_type.width()), v->val());
	} else {
		// This is an extension
		if (vtype.type_class() == value_type_class::signed_integer) {
			// The value is a signed integer, so do a sign extension.
			r = builder_.insert_sx(value_type(value_type_class::signed_integer, target_type.width()), v->val());
		} else if (vtype.type_class() == value_type_class::unsigned_integer) {
			// The value is an unsigned integer, so do a zero extension.
			r = builder_.insert_zx(value_type(value_type_class::unsigned_integer, target_type.width()), v->val());
		} else {
			// Other type classes (void, floating point, etc) are not supported.
			throw std::runtime_error("auto cast not supported");
		}
	}

	// We've done an extension or a truncation, but we might also have to do a bitcast,
	// e.g. if we're sign extensing a s32 to u64, we have to sign extend, but then bitcast.
	if (target_type.type_class() != vtype.type_class()) {
		r = builder_.insert_bitcast(target_type, r->val());
	}

	return r;
}

value_type translator::type_of_operand(int opnum)
{
	const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst());
	auto operand = xed_inst_operand(insn, opnum);
	auto opname = xed_operand_name(operand);

	switch (opname) {
	case XED_OPERAND_REG0:
	case XED_OPERAND_REG1: {
		auto reg = xed_decoded_inst_get_reg(xed_inst(), opname);
		auto regclass = xed_reg_class(reg);

		switch (regclass) {
		case XED_REG_CLASS_GPR:
			return value_type(value_type_class::unsigned_integer, xed_get_register_width_bits(reg));

		case XED_REG_CLASS_XMM:
			return value_type(value_type_class::unsigned_integer, 128);

		default:
			throw std::runtime_error("unsupported register class");
		}
	}

	case XED_OPERAND_IMM0: {
		return value_type(value_type_class::signed_integer, xed_decoded_inst_get_immediate_width_bits(xed_inst()));
	}

	default:
		throw std::logic_error("unsupported operand type: " + std::to_string((int)opname));
	}
}
