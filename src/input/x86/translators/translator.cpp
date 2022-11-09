#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/port.h>

#include <csignal>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void translator::translate(off_t address, xed_decoded_inst_t *xed_inst)
{
	char buffer[64];
	xed_format_context(XED_SYNTAX_ATT, xed_inst, buffer, sizeof(buffer) - 1, address, nullptr, 0);

	builder_.begin_packet(address, std::string(buffer));

	xed_inst_ = xed_inst;
	do_translate();

	builder_.end_packet();
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
		case XED_REG_CLASS_GPR:
			if (value.type().width() == 64) {
				if (value.type().type_class() == value_type_class::signed_integer) {
					return write_reg(reg_to_offset(reg), builder_.insert_bitcast(value_type::u64(), value)->val());
				} else {
					return write_reg(reg_to_offset(reg), value);
				}
			} else {
				// TODO: AH behaviour
				if (value.type().width() == 32) {
					// EAX behaviour
					return write_reg(reg_to_offset(reg), builder_.insert_zx(value_type::u64(), value)->val());
				} else {
					// AX/AL behaviour
					auto orig = read_reg(value_type::u32(), reg_to_offset(reg));
					auto repl = builder_.insert_or(orig->val(), builder_.insert_zx(value_type::u32(), value)->val());

					return write_reg(reg_to_offset(reg), builder_.insert_zx(value_type::u64(), repl->val())->val());
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

		switch (xed_decoded_inst_get_operand_width(xed_inst())) {
		case 8:
			return builder_.insert_read_mem(value_type::u8(), addr->val());
		case 16:
			return builder_.insert_read_mem(value_type::u16(), addr->val());
		case 32:
			return builder_.insert_read_mem(value_type::u32(), addr->val());
		case 64:
			return builder_.insert_read_mem(value_type::u64(), addr->val());

		default:
			throw std::runtime_error("invalid memory width in read");
		}
	}

	default:
		throw std::logic_error("unsupported read operand type: " + std::to_string((int)opname));
	}
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
		address_base = builder_.insert_add(address_base->val(), read_reg(value_type::u64(), reg_offsets::fs)->val());
	} else if (seg == XED_REG_GS) {
		address_base = builder_.insert_add(address_base->val(), read_reg(value_type::u64(), reg_offsets::gs)->val());
	}

	return address_base;
}

translator::reg_offsets translator::reg_to_offset(xed_reg_enum_t reg)
{
	switch (xed_reg_class(reg)) {
	case XED_REG_CLASS_GPR:
		return (reg_offsets)((xed_get_largest_enclosing_register(reg) - XED_REG_RAX) + (int)reg_offsets::rax);

	case XED_REG_CLASS_XMM:
		return (reg_offsets)((reg - XED_REG_XMM0) + (int)reg_offsets::xmm0);

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
		write_reg(reg_offsets::zf, builder_.insert_constant_i(value_type::u1(), 0)->val());
		break;

	case flag_op::set1:
		write_reg(reg_offsets::zf, builder_.insert_constant_i(value_type::u1(), 1)->val());
		break;

	case flag_op::update:
		if (op->kind() == node_kinds::unary_arith || op->kind() == node_kinds::binary_arith || op->kind() == node_kinds::ternary_arith) {
			write_reg(reg_offsets::zf, ((arith_node *)op)->zero());
		} else if (op->kind() == node_kinds::constant) {
			write_reg(reg_offsets::zf, builder_.insert_constant_i(value_type::u1(), ((constant_node *)op)->is_zero())->val());
		} else {
			throw std::runtime_error("unsupported operation node type for ZF");
		}
		break;

	default:
		break;
	}

	switch (cf) {
	case flag_op::set0:
		write_reg(reg_offsets::cf, builder_.insert_constant_i(value_type::u1(), 0)->val());
		break;

	case flag_op::set1:
		write_reg(reg_offsets::cf, builder_.insert_constant_i(value_type::u1(), 1)->val());
		break;

	case flag_op::update:
		if (op->kind() == node_kinds::binary_arith || op->kind() == node_kinds::ternary_arith) {
			write_reg(reg_offsets::cf, ((arith_node *)op)->carry());
		} else {
			throw std::runtime_error("unsupported operation node type for CF");
		}
		break;

	default:
		break;
	}

	switch (of) {
	case flag_op::set0:
		write_reg(reg_offsets::of, builder_.insert_constant_i(value_type::u1(), 0)->val());
		break;

	case flag_op::set1:
		write_reg(reg_offsets::of, builder_.insert_constant_i(value_type::u1(), 1)->val());
		break;

	case flag_op::update:
		if (op->kind() == node_kinds::binary_arith || op->kind() == node_kinds::ternary_arith) {
			write_reg(reg_offsets::of, ((arith_node *)op)->overflow());
		} else {
			throw std::runtime_error("unsupported operation node type for OF");
		}
		break;

	default:
		break;
	}

	switch (sf) {
	case flag_op::set0:
		write_reg(reg_offsets::sf, builder_.insert_constant_i(value_type::u1(), 0)->val());
		break;

	case flag_op::set1:
		write_reg(reg_offsets::sf, builder_.insert_constant_i(value_type::u1(), 1)->val());
		break;

	case flag_op::update:
		if (op->kind() == node_kinds::unary_arith || op->kind() == node_kinds::binary_arith || op->kind() == node_kinds::ternary_arith) {
			write_reg(reg_offsets::sf, ((arith_node *)op)->negative());
		} else if (op->kind() == node_kinds::constant) {
			// TODO: INCORRECT
			write_reg(reg_offsets::sf, builder_.insert_constant_i(value_type::u1(), ((constant_node *)op)->const_val_i() < 0)->val());
		} else {
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
		auto ncf = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::cf)->val());
		auto nzf = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::zf)->val());
		return builder_.insert_and(ncf->val(), nzf->val());
	}

	case cond_type::nb: {
		auto ncf = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::cf)->val());
		return ncf;
	}

	case cond_type::b: {
		auto cf = read_reg(value_type::u1(), reg_offsets::cf);
		return cf;
	}

	case cond_type::be: {
		auto cf = read_reg(value_type::u1(), reg_offsets::cf);
		auto zf = read_reg(value_type::u1(), reg_offsets::zf);
		return builder_.insert_or(cf->val(), zf->val());
	}

	case cond_type::z: {
		auto zf = read_reg(value_type::u1(), reg_offsets::zf);
		return zf;
	}

	case cond_type::nle: {
		auto nzf = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::zf)->val());
		auto sf = read_reg(value_type::u1(), reg_offsets::sf);
		auto of = read_reg(value_type::u1(), reg_offsets::of);

		return builder_.insert_and(nzf->val(), builder_.insert_cmpeq(sf->val(), of->val())->val());
	}

	case cond_type::nl: {
		auto sf = read_reg(value_type::u1(), reg_offsets::sf);
		auto of = read_reg(value_type::u1(), reg_offsets::of);

		return builder_.insert_cmpeq(sf->val(), of->val());
	}

	case cond_type::l: {
		auto sf = read_reg(value_type::u1(), reg_offsets::sf);
		auto of = read_reg(value_type::u1(), reg_offsets::of);

		return builder_.insert_cmpne(sf->val(), of->val());
	}

	case cond_type::le: {
		auto zf = read_reg(value_type::u1(), reg_offsets::zf);
		auto sf = read_reg(value_type::u1(), reg_offsets::sf);
		auto of = read_reg(value_type::u1(), reg_offsets::of);

		return builder_.insert_or(zf->val(), builder_.insert_cmpne(sf->val(), of->val())->val());
	}

	case cond_type::nz: {
		auto nzf = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::zf)->val());
		return nzf;
	}

	case cond_type::no: {
		auto nof = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::of)->val());
		return nof;
	}

	case cond_type::np: {
		auto npf = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::pf)->val());
		return npf;
	}

	case cond_type::ns: {
		auto nsf = builder_.insert_not(read_reg(value_type::u1(), reg_offsets::sf)->val());
		return nsf;
	}

	case cond_type::o: {
		auto of = read_reg(value_type::u1(), reg_offsets::of);
		return of;
	}

	case cond_type::p: {
		auto pf = read_reg(value_type::u1(), reg_offsets::pf);
		return pf;
	}

	case cond_type::s: {
		auto sf = read_reg(value_type::u1(), reg_offsets::sf);
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
