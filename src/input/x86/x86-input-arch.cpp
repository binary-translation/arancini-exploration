#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/ir/chunk.h>
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

#define REGOFF_RAX 1
#define REGOFF_ZF 17
#define REGOFF_CF 18
#define REGOFF_OF 19
#define REGOFF_SF 20
#define REGOFF_PF 21
#define REGOFF_XMM0 22
#define REGOFF_FS 38
#define REGOFF_GS 39

static int reg_to_offset(xed_reg_enum_t reg)
{
	switch (xed_reg_class(reg)) {
	case XED_REG_CLASS_GPR:
		return (xed_get_largest_enclosing_register(reg) - XED_REG_RAX) + REGOFF_RAX;

	case XED_REG_CLASS_XMM:
		return (reg - XED_REG_XMM0) + REGOFF_XMM0;

	default:
		throw std::runtime_error("unsupported register class when computing offset");
	}
}

static value_node *compute_address(std::shared_ptr<packet> pkt, xed_decoded_inst_t *xed_inst, int mem_idx)
{
	auto base_reg = xed_decoded_inst_get_base_reg(xed_inst, mem_idx);
	auto displ = xed_decoded_inst_get_memory_displacement(xed_inst, mem_idx);

	auto index = xed_decoded_inst_get_index_reg(xed_inst, mem_idx);
	auto scale = xed_decoded_inst_get_scale(xed_inst, mem_idx);

	auto seg = xed_decoded_inst_get_seg_reg(xed_inst, mem_idx);

	if (xed_get_register_width_bits(base_reg) != 64 && base_reg != XED_REG_INVALID) {
		throw std::runtime_error("base reg invalid size");
	}

	value_node *address_base;

	if (base_reg == XED_REG_INVALID) {
		address_base = pkt->insert_constant_u64(0);
	} else if (base_reg == XED_REG_RIP) {
		address_base = pkt->insert_read_pc();
	} else {
		address_base = pkt->insert_read_reg(value_type::u64(), reg_to_offset(base_reg));
	}

	if (index != XED_REG_INVALID) {
		auto scaled_index = pkt->insert_mul(pkt->insert_read_reg(value_type::u64(), reg_to_offset(index))->val(), pkt->insert_constant_u64(scale)->val());

		address_base = pkt->insert_add(address_base->val(), scaled_index->val());
	}

	if (displ) {
		address_base = pkt->insert_add(address_base->val(), pkt->insert_constant_u64(displ)->val());
	}

	if (seg == XED_REG_FS) {
		address_base = pkt->insert_add(address_base->val(), pkt->insert_read_reg(value_type::u64(), REGOFF_FS)->val());
	} else if (seg == XED_REG_GS) {
		address_base = pkt->insert_add(address_base->val(), pkt->insert_read_reg(value_type::u64(), REGOFF_GS)->val());
	}

	return address_base;
}

static value_node *read_operand(std::shared_ptr<packet> pkt, xed_decoded_inst_t *xed_inst, int opnum)
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
			switch (xed_get_register_width_bits(reg)) {
			case 8:
				return pkt->insert_read_reg(value_type::u8(), reg_to_offset(reg));
			case 16:
				return pkt->insert_read_reg(value_type::u16(), reg_to_offset(reg));
			case 32:
				return pkt->insert_read_reg(value_type::u32(), reg_to_offset(reg));
			case 64:
				return pkt->insert_read_reg(value_type::u64(), reg_to_offset(reg));
			default:
				throw std::runtime_error("unsupported register size");
			}

		case XED_REG_CLASS_XMM:
			return pkt->insert_read_reg(value_type::u128(), reg_to_offset(reg));

		default:
			throw std::runtime_error("unsupported register class");
		}
	}

	case XED_OPERAND_IMM0: {
		switch (xed_decoded_inst_get_immediate_width_bits(xed_inst)) {
		case 8:
			return pkt->insert_constant_s32(xed_decoded_inst_get_signed_immediate(xed_inst));

		case 16:
			return pkt->insert_constant_s32(xed_decoded_inst_get_signed_immediate(xed_inst));

		case 32:
			return pkt->insert_constant_s32(xed_decoded_inst_get_signed_immediate(xed_inst));

		case 64:
			return pkt->insert_constant_s32(xed_decoded_inst_get_signed_immediate(xed_inst));

		default:
			throw std::runtime_error("unsupported immediate width");
		}
	}

	case XED_OPERAND_MEM0: {
		auto addr = compute_address(pkt, xed_inst, 0);

		switch (xed_decoded_inst_get_operand_width(xed_inst)) {
		case 8:
			return pkt->insert_read_mem(value_type::u8(), addr->val());
		case 16:
			return pkt->insert_read_mem(value_type::u16(), addr->val());
		case 32:
			return pkt->insert_read_mem(value_type::u32(), addr->val());
		case 64:
			return pkt->insert_read_mem(value_type::u64(), addr->val());

		default:
			throw std::runtime_error("invalid memory width in read");
		}
	}

	default:
		throw std::logic_error("unsupported read operand type: " + std::to_string((int)opname));
	}
}

static action_node *write_operand(std::shared_ptr<packet> pkt, xed_decoded_inst_t *xed_inst, int opnum, port &value)
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
			if (value.type().width() == 64) {
				if (value.type().type_class() == value_type_class::signed_integer) {
					return pkt->insert_write_reg(reg_to_offset(reg), pkt->insert_bitcast(value_type::u64(), value)->val());
				} else {
					return pkt->insert_write_reg(reg_to_offset(reg), value);
				}
			} else {
				// TODO: AH behaviour
				if (value.type().width() == 32) {
					// EAX behaviour
					return pkt->insert_write_reg(reg_to_offset(reg), pkt->insert_zx(value_type::u64(), value)->val());
				} else {
					// AX/AL behaviour
					auto orig = pkt->insert_read_reg(value_type::u32(), reg_to_offset(reg));
					auto repl = pkt->insert_or(orig->val(), pkt->insert_zx(value_type::u32(), value)->val());

					return pkt->insert_write_reg(reg_to_offset(reg), pkt->insert_zx(value_type::u64(), repl->val())->val());
				}
			}

		case XED_REG_CLASS_XMM:
			return pkt->insert_write_reg(reg_to_offset(reg), pkt->insert_zx(value_type::u128(), value)->val());

		default:
			throw std::runtime_error("unsupported register class");
		}
	}

	case XED_OPERAND_MEM0: {
		auto address = compute_address(pkt, xed_inst, 0);
		return pkt->insert_write_mem(address->val(), value);
	}

	default:
		throw std::logic_error("unsupported write operand type: " + std::to_string((int)opname));
	}
}

enum flag_op { ignore, set0, set1, update };

static void write_flags(
	std::shared_ptr<packet> pkt, xed_decoded_inst_t *xed_inst, value_node *op, flag_op zf, flag_op cf, flag_op of, flag_op sf, flag_op pf, flag_op af)
{
	switch (zf) {
	case flag_op::set0:
		pkt->insert_write_reg(REGOFF_ZF, pkt->insert_constant(value_type::u1(), 0)->val());
		break;

	case flag_op::set1:
		pkt->insert_write_reg(REGOFF_ZF, pkt->insert_constant(value_type::u1(), 1)->val());
		break;

	case flag_op::update:
		if (op->kind() == node_kinds::binary_arith || op->kind() == node_kinds::ternary_arith) {
			pkt->insert_write_reg(REGOFF_ZF, ((arith_node *)op)->zero());
		} else if (op->kind() == node_kinds::constant) {
			pkt->insert_write_reg(REGOFF_ZF, pkt->insert_constant(value_type::u1(), ((constant_node *)op)->const_val() == 0)->val());
		} else {
			throw std::runtime_error("unsupported operation node type for ZF");
		}
		break;

	default:
		break;
	}

	switch (cf) {
	case flag_op::set0:
		pkt->insert_write_reg(REGOFF_CF, pkt->insert_constant(value_type::u1(), 0)->val());
		break;

	case flag_op::set1:
		pkt->insert_write_reg(REGOFF_CF, pkt->insert_constant(value_type::u1(), 1)->val());
		break;

	case flag_op::update:
		if (op->kind() == node_kinds::binary_arith || op->kind() == node_kinds::ternary_arith) {
			pkt->insert_write_reg(REGOFF_CF, ((arith_node *)op)->carry());
		} else {
			throw std::runtime_error("unsupported operation node type for CF");
		}
		break;

	default:
		break;
	}

	switch (of) {
	case flag_op::set0:
		pkt->insert_write_reg(REGOFF_OF, pkt->insert_constant(value_type::u1(), 0)->val());
		break;

	case flag_op::set1:
		pkt->insert_write_reg(REGOFF_OF, pkt->insert_constant(value_type::u1(), 1)->val());
		break;

	case flag_op::update:
		if (op->kind() == node_kinds::binary_arith || op->kind() == node_kinds::ternary_arith) {
			pkt->insert_write_reg(REGOFF_OF, ((arith_node *)op)->overflow());
		} else {
			throw std::runtime_error("unsupported operation node type for OF");
		}
		break;

	default:
		break;
	}

	switch (sf) {
	case flag_op::set0:
		pkt->insert_write_reg(REGOFF_SF, pkt->insert_constant(value_type::u1(), 0)->val());
		break;

	case flag_op::set1:
		pkt->insert_write_reg(REGOFF_SF, pkt->insert_constant(value_type::u1(), 1)->val());
		break;

	case flag_op::update:
		if (op->kind() == node_kinds::binary_arith || op->kind() == node_kinds::ternary_arith) {
			pkt->insert_write_reg(REGOFF_SF, ((arith_node *)op)->negative());
		} else if (op->kind() == node_kinds::constant) {
			// TODO: INCORRECT
			pkt->insert_write_reg(REGOFF_SF, pkt->insert_constant(value_type::u1(), ((constant_node *)op)->const_val() < 0)->val());
		} else {
			throw std::runtime_error("unsupported operation node type for SF");
		}
		break;

	default:
		break;
	}
}

enum class cond_type { nbe, nb, b, be, z, nle, nl, l, le, nz, no, np, ns, o, p, s };

static value_node *compute_cond(std::shared_ptr<packet> pkt, cond_type ct)
{
	switch (ct) {
	case cond_type::nbe: {
		auto ncf = pkt->insert_not(pkt->insert_read_reg(value_type::u1(), REGOFF_CF)->val());
		auto nzf = pkt->insert_not(pkt->insert_read_reg(value_type::u1(), REGOFF_ZF)->val());
		return pkt->insert_and(ncf->val(), nzf->val());
	}

	case cond_type::nb: {
		auto ncf = pkt->insert_not(pkt->insert_read_reg(value_type::u1(), REGOFF_CF)->val());
		return ncf;
	}

	case cond_type::b: {
		auto cf = pkt->insert_read_reg(value_type::u1(), REGOFF_CF);
		return cf;
	}

	case cond_type::be: {
		auto cf = pkt->insert_read_reg(value_type::u1(), REGOFF_CF);
		auto zf = pkt->insert_read_reg(value_type::u1(), REGOFF_ZF);
		return pkt->insert_or(cf->val(), zf->val());
	}

	case cond_type::z: {
		auto zf = pkt->insert_read_reg(value_type::u1(), REGOFF_ZF);
		return zf;
	}

	case cond_type::nle: {
		auto nzf = pkt->insert_not(pkt->insert_read_reg(value_type::u1(), REGOFF_ZF)->val());
		auto sf = pkt->insert_read_reg(value_type::u1(), REGOFF_SF);
		auto of = pkt->insert_read_reg(value_type::u1(), REGOFF_OF);

		return pkt->insert_and(nzf->val(), pkt->insert_cmpeq(sf->val(), of->val())->val());
	}

	case cond_type::nl: {
		auto sf = pkt->insert_read_reg(value_type::u1(), REGOFF_SF);
		auto of = pkt->insert_read_reg(value_type::u1(), REGOFF_OF);

		return pkt->insert_cmpeq(sf->val(), of->val());
	}

	case cond_type::l: {
		auto sf = pkt->insert_read_reg(value_type::u1(), REGOFF_SF);
		auto of = pkt->insert_read_reg(value_type::u1(), REGOFF_OF);

		return pkt->insert_cmpne(sf->val(), of->val());
	}

	case cond_type::le: {
		auto zf = pkt->insert_read_reg(value_type::u1(), REGOFF_ZF);
		auto sf = pkt->insert_read_reg(value_type::u1(), REGOFF_SF);
		auto of = pkt->insert_read_reg(value_type::u1(), REGOFF_OF);

		return pkt->insert_or(zf->val(), pkt->insert_cmpne(sf->val(), of->val())->val());
	}

	case cond_type::nz: {
		auto nzf = pkt->insert_not(pkt->insert_read_reg(value_type::u1(), REGOFF_ZF)->val());
		return nzf;
	}

	case cond_type::no: {
		auto nof = pkt->insert_not(pkt->insert_read_reg(value_type::u1(), REGOFF_OF)->val());
		return nof;
	}

	case cond_type::np: {
		auto npf = pkt->insert_not(pkt->insert_read_reg(value_type::u1(), REGOFF_PF)->val());
		return npf;
	}

	case cond_type::ns: {
		auto nsf = pkt->insert_not(pkt->insert_read_reg(value_type::u1(), REGOFF_SF)->val());
		return nsf;
	}

	case cond_type::o: {
		auto of = pkt->insert_read_reg(value_type::u1(), REGOFF_OF);
		return of;
	}

	case cond_type::p: {
		auto pf = pkt->insert_read_reg(value_type::u1(), REGOFF_PF);
		return pf;
	}

	case cond_type::s: {
		auto sf = pkt->insert_read_reg(value_type::u1(), REGOFF_SF);
		return sf;
	}

	default:
		throw std::runtime_error("unhandled condition code");
	}
}

static void handle_jcc(std::shared_ptr<packet> pkt, xed_decoded_inst_t *xed_inst)
{
	value_node *cond = nullptr;
	switch (xed_decoded_inst_get_iclass(xed_inst)) {

	case XED_ICLASS_JNBE:
		cond = compute_cond(pkt, cond_type::nbe);
		break;
	case XED_ICLASS_JNB:
		cond = compute_cond(pkt, cond_type::nb);
		break;
	case XED_ICLASS_JB:
		cond = compute_cond(pkt, cond_type::b);
		break;
	case XED_ICLASS_JBE:
		cond = compute_cond(pkt, cond_type::be);
		break;
	case XED_ICLASS_JZ:
		cond = compute_cond(pkt, cond_type::z);
		break;
	case XED_ICLASS_JNLE:
		cond = compute_cond(pkt, cond_type::nle);
		break;
	case XED_ICLASS_JNL:
		cond = compute_cond(pkt, cond_type::nl);
		break;
	case XED_ICLASS_JL:
		cond = compute_cond(pkt, cond_type::l);
		break;
	case XED_ICLASS_JLE:
		cond = compute_cond(pkt, cond_type::le);
		break;
	case XED_ICLASS_JNZ:
		cond = compute_cond(pkt, cond_type::nz);
		break;
	case XED_ICLASS_JNO:
		cond = compute_cond(pkt, cond_type::no);
		break;
	case XED_ICLASS_JNP:
		cond = compute_cond(pkt, cond_type::np);
		break;
	case XED_ICLASS_JNS:
		cond = compute_cond(pkt, cond_type::ns);
		break;
	case XED_ICLASS_JO:
		cond = compute_cond(pkt, cond_type::o);
		break;
	case XED_ICLASS_JP:
		cond = compute_cond(pkt, cond_type::p);
		break;
	case XED_ICLASS_JS:
		cond = compute_cond(pkt, cond_type::s);
		break;

	default:
		throw std::runtime_error("unhandled jump instruction");
	}

	xed_uint_t instruction_length = xed_decoded_inst_get_length(xed_inst);
	auto fallthrough = pkt->insert_add(pkt->insert_read_pc()->val(), pkt->insert_constant_u64(instruction_length)->val());

	int32_t branch_displacement = xed_decoded_inst_get_branch_displacement(xed_inst);
	uint64_t branch_target = branch_displacement + instruction_length;

	auto target = pkt->insert_add(pkt->insert_read_pc()->val(), pkt->insert_constant_u64(branch_target)->val());

	pkt->insert_write_pc(pkt->insert_csel(cond->val(), target->val(), fallthrough->val())->val());
}

static void handle_setcc(std::shared_ptr<packet> pkt, xed_decoded_inst_t *xed_inst)
{
	value_node *cond = nullptr;
	switch (xed_decoded_inst_get_iclass(xed_inst)) {

	case XED_ICLASS_SETNBE:
		cond = compute_cond(pkt, cond_type::nbe);
		break;
	case XED_ICLASS_SETNB:
		cond = compute_cond(pkt, cond_type::nb);
		break;
	case XED_ICLASS_SETB:
		cond = compute_cond(pkt, cond_type::b);
		break;
	case XED_ICLASS_SETBE:
		cond = compute_cond(pkt, cond_type::be);
		break;
	case XED_ICLASS_SETZ:
		cond = compute_cond(pkt, cond_type::z);
		break;
	case XED_ICLASS_SETNLE:
		cond = compute_cond(pkt, cond_type::nle);
		break;
	case XED_ICLASS_SETNL:
		cond = compute_cond(pkt, cond_type::nl);
		break;
	case XED_ICLASS_SETL:
		cond = compute_cond(pkt, cond_type::l);
		break;
	case XED_ICLASS_SETLE:
		cond = compute_cond(pkt, cond_type::le);
		break;
	case XED_ICLASS_SETNZ:
		cond = compute_cond(pkt, cond_type::nz);
		break;
	case XED_ICLASS_SETNO:
		cond = compute_cond(pkt, cond_type::no);
		break;
	case XED_ICLASS_SETNP:
		cond = compute_cond(pkt, cond_type::np);
		break;
	case XED_ICLASS_SETNS:
		cond = compute_cond(pkt, cond_type::ns);
		break;
	case XED_ICLASS_SETO:
		cond = compute_cond(pkt, cond_type::o);
		break;
	case XED_ICLASS_SETP:
		cond = compute_cond(pkt, cond_type::p);
		break;
	case XED_ICLASS_SETS:
		cond = compute_cond(pkt, cond_type::s);
		break;

	default:
		throw std::runtime_error("unhandled jump instruction");
	}

	write_operand(pkt, xed_inst, 0, cond->val());
}

/*
 * Automatically casts the value to the target type, taking into account sign/zero extension/truncation rules.
 */
static value_node *auto_cast(std::shared_ptr<packet> pkt, const value_type &target_type, value_node *v)
{
	const auto &vtype = v->val().type();

	// If the widths of the type are the same, then we might only have to do a bitcast.
	if (target_type.width() == vtype.width()) {
		if (target_type.type_class() != vtype.type_class()) {
			// If the type classes are different, then just do a bitcast.
			return pkt->insert_bitcast(target_type, v->val());
		} else {
			// Otherwise, there's actually nothing to do.
			return v;
		}
	}

	// If we get here, we know we need to do a truncation, or an extension.
	value_node *r;

	if (target_type.width() < vtype.width()) {
		// This is a truncation
		r = pkt->insert_trunc(value_type(vtype.type_class(), target_type.width()), v->val());
	} else {
		// This is an extension
		if (vtype.type_class() == value_type_class::signed_integer) {
			// The value is a signed integer, so do a sign extension.
			r = pkt->insert_sx(value_type(value_type_class::signed_integer, target_type.width()), v->val());
		} else if (vtype.type_class() == value_type_class::unsigned_integer) {
			// The value is an unsigned integer, so do a zero extension.
			r = pkt->insert_zx(value_type(value_type_class::unsigned_integer, target_type.width()), v->val());
		} else {
			// Other type classes (void, floating point, etc) are not supported.
			throw std::runtime_error("auto cast not supported");
		}
	}

	// We've done an extension or a truncation, but we might also have to do a bitcast,
	// e.g. if we're sign extensing a s32 to u64, we have to sign extend, but then bitcast.
	if (target_type.type_class() != vtype.type_class()) {
		r = pkt->insert_bitcast(target_type, r->val());
	}

	return r;
}

static value_type type_of_operand(xed_decoded_inst_t *xed_inst, int opnum)
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
			return value_type(value_type_class::unsigned_integer, xed_get_register_width_bits(reg));

		case XED_REG_CLASS_XMM:
			return value_type(value_type_class::unsigned_integer, 128);

		default:
			throw std::runtime_error("unsupported register class");
		}
	}

	case XED_OPERAND_IMM0: {
		return value_type(value_type_class::signed_integer, xed_decoded_inst_get_immediate_width_bits(xed_inst));
	}

	default:
		throw std::logic_error("unsupported operand type: " + std::to_string((int)opname));
	}
}

static std::shared_ptr<packet> translate_instruction(off_t address, xed_decoded_inst_t *xed_inst)
{
	auto pkt = std::make_shared<packet>();
	pkt->insert_start(address);

	char buffer[64];
	// xed_decoded_inst_dump(xed_inst, buffer, sizeof(buffer));
	xed_format_context(XED_SYNTAX_INTEL, xed_inst, buffer, sizeof(buffer), address, nullptr, 0);

	std::cerr << "insn @ " << std::hex << address << ": " << buffer << std::endl;

	switch (xed_decoded_inst_get_iclass(xed_inst)) {
	case XED_ICLASS_XOR: {
		auto op0 = read_operand(pkt, xed_inst, 0);
		auto op1 = auto_cast(pkt, op0->val().type(), read_operand(pkt, xed_inst, 1));
		auto rslt = pkt->insert_xor(op0->val(), op1->val());

		write_operand(pkt, xed_inst, 0, rslt->val());
		write_flags(pkt, xed_inst, rslt, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::ignore);
		break;
	}

	case XED_ICLASS_AND: {
		auto op0 = read_operand(pkt, xed_inst, 0);
		auto op1 = auto_cast(pkt, op0->val().type(), read_operand(pkt, xed_inst, 1));
		auto rslt = pkt->insert_and(op0->val(), op1->val());

		write_operand(pkt, xed_inst, 0, rslt->val());
		write_flags(pkt, xed_inst, rslt, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::ignore);
		break;
	}

	case XED_ICLASS_OR: {
		auto op0 = read_operand(pkt, xed_inst, 0);
		auto op1 = auto_cast(pkt, op0->val().type(), read_operand(pkt, xed_inst, 1));
		auto rslt = pkt->insert_or(op0->val(), op1->val());

		write_operand(pkt, xed_inst, 0, rslt->val());
		write_flags(pkt, xed_inst, rslt, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::ignore);
		break;
	}

	case XED_ICLASS_NOT: {
		auto op0 = read_operand(pkt, xed_inst, 0);
		auto rslt = pkt->insert_not(op0->val());

		write_operand(pkt, xed_inst, 0, rslt->val());
		break;
	}

	case XED_ICLASS_SUB: {
		auto op0 = read_operand(pkt, xed_inst, 0);
		auto op1 = auto_cast(pkt, op0->val().type(), read_operand(pkt, xed_inst, 1));
		auto rslt = pkt->insert_sub(op0->val(), op1->val());

		write_operand(pkt, xed_inst, 0, rslt->val());
		write_flags(pkt, xed_inst, rslt, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::update);
		break;
	}

	case XED_ICLASS_SBB: {
		auto op0 = read_operand(pkt, xed_inst, 0);
		auto op1 = auto_cast(pkt, op0->val().type(), read_operand(pkt, xed_inst, 1));
		auto cf = auto_cast(pkt, op0->val().type(), pkt->insert_read_reg(value_type::u1(), REGOFF_CF));
		auto rslt = pkt->insert_sbb(op0->val(), op1->val(), cf->val());

		write_operand(pkt, xed_inst, 0, rslt->val());
		write_flags(pkt, xed_inst, rslt, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::update);
		break;
	}

	case XED_ICLASS_CMP: {
		auto op0 = read_operand(pkt, xed_inst, 0);
		auto op1 = auto_cast(pkt, op0->val().type(), read_operand(pkt, xed_inst, 1));
		auto rslt = pkt->insert_sub(op0->val(), op1->val());

		write_flags(pkt, xed_inst, rslt, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::update);
		break;
	}

	case XED_ICLASS_ADD: {
		auto op0 = read_operand(pkt, xed_inst, 0);
		auto op1 = auto_cast(pkt, op0->val().type(), read_operand(pkt, xed_inst, 1));
		auto rslt = pkt->insert_add(op0->val(), op1->val());

		write_operand(pkt, xed_inst, 0, rslt->val());
		write_flags(pkt, xed_inst, rslt, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::update);
		break;
	}

	case XED_ICLASS_IMUL: {
		auto op0 = read_operand(pkt, xed_inst, 0);
		auto op1 = read_operand(pkt, xed_inst, 1);
		auto rslt = pkt->insert_mul(op0->val(), op1->val());

		write_operand(pkt, xed_inst, 0, rslt->val());
		write_flags(pkt, xed_inst, rslt, flag_op::ignore, flag_op::set0, flag_op::set0, flag_op::ignore, flag_op::ignore, flag_op::ignore);
		break;
	}

	case XED_ICLASS_IDIV: {
		auto op0 = read_operand(pkt, xed_inst, 0);
		auto op1 = read_operand(pkt, xed_inst, 1);
		auto rslt = pkt->insert_div(op0->val(), op1->val());

		write_operand(pkt, xed_inst, 0, rslt->val());
		write_flags(pkt, xed_inst, rslt, flag_op::ignore, flag_op::set0, flag_op::set0, flag_op::ignore, flag_op::ignore, flag_op::ignore);
		break;
	}

	case XED_ICLASS_MOV: {
		const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst);
		auto operand = xed_inst_operand(insn, 0);
		auto opname = xed_operand_name(operand);

		auto tt = opname == XED_OPERAND_MEM0 ? type_of_operand(xed_inst, 1) : type_of_operand(xed_inst, 0);
		auto op1 = auto_cast(pkt, tt, read_operand(pkt, xed_inst, 1));
		write_operand(pkt, xed_inst, 0, op1->val());
		break;
	}

	case XED_ICLASS_MOVQ: {
		// TODO: INCORRECT FOR SOME SIZES
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

		break;
	}

	case XED_ICLASS_TEST: {
		auto op0 = read_operand(pkt, xed_inst, 0);
		auto op1 = auto_cast(pkt, op0->val().type(), read_operand(pkt, xed_inst, 1));
		auto rslt = pkt->insert_and(op0->val(), op1->val());

		write_flags(pkt, xed_inst, rslt, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::ignore);
		break;
	}

	case XED_ICLASS_MOVSXD: {
		// TODO: INCORRECT FOR SOME SIZES
		auto input = read_operand(pkt, xed_inst, 1);
		input = pkt->insert_bitcast(value_type(value_type_class::signed_integer, input->val().type().width()), input->val());

		auto cast = pkt->insert_sx(value_type::s64(), input->val());

		write_operand(pkt, xed_inst, 0, cast->val());
		break;
	}

	case XED_ICLASS_MOVZX: {
		// TODO: Incorrect operand sizes
		auto input = read_operand(pkt, xed_inst, 1);
		auto cast = pkt->insert_zx(value_type::u64(), input->val());

		write_operand(pkt, xed_inst, 0, cast->val());
		break;
	}

	case XED_ICLASS_MOVHPS:
	case XED_ICLASS_MOVUPS: {
		// TODO: INCORRECT FOR SOME SIZES

		auto src = read_operand(pkt, xed_inst, 1);
		// auto dst = read_operand(pkt, xed_inst, 0);
		// auto result = pkt->insert_or(dst->val(), )
		write_operand(pkt, xed_inst, 0, src->val());
		break;
	}

	case XED_ICLASS_LEA: {
		auto addr = compute_address(pkt, xed_inst, 0);
		write_operand(pkt, xed_inst, 0, addr->val());
		break;
	}

	case XED_ICLASS_JNBE:
	case XED_ICLASS_JNB:
	case XED_ICLASS_JB:
	case XED_ICLASS_JBE:
	case XED_ICLASS_JZ:
	case XED_ICLASS_JNLE:
	case XED_ICLASS_JNL:
	case XED_ICLASS_JL:
	case XED_ICLASS_JLE:
	case XED_ICLASS_JNZ:
	case XED_ICLASS_JNO:
	case XED_ICLASS_JNP:
	case XED_ICLASS_JNS:
	case XED_ICLASS_JO:
	case XED_ICLASS_JP:
	case XED_ICLASS_JS:
		handle_jcc(pkt, xed_inst);
		break;

	case XED_ICLASS_SETNBE:
	case XED_ICLASS_SETNB:
	case XED_ICLASS_SETB:
	case XED_ICLASS_SETBE:
	case XED_ICLASS_SETZ:
	case XED_ICLASS_SETNLE:
	case XED_ICLASS_SETNL:
	case XED_ICLASS_SETL:
	case XED_ICLASS_SETLE:
	case XED_ICLASS_SETNZ:
	case XED_ICLASS_SETNO:
	case XED_ICLASS_SETNP:
	case XED_ICLASS_SETNS:
	case XED_ICLASS_SETO:
	case XED_ICLASS_SETP:
	case XED_ICLASS_SETS:
		handle_setcc(pkt, xed_inst);
		break;

	case XED_ICLASS_JMP: {
		xed_uint_t instruction_length = xed_decoded_inst_get_length(xed_inst);
		int32_t branch_displacement = xed_decoded_inst_get_branch_displacement(xed_inst);
		uint64_t branch_target = branch_displacement + instruction_length;

		auto target = pkt->insert_add(pkt->insert_read_pc()->val(), pkt->insert_constant_u64(branch_target)->val());

		pkt->insert_write_pc(target->val());

		break;
	}

	case XED_ICLASS_CMOVNS: {
		// Not Sign: SF==0

		/*
		temp ← SRC
			IF condition TRUE
				THEN
					DEST ← temp;
				FI;
			ELSE
				IF (OperandSize = 32 and IA-32e mode active)
					THEN
						DEST[63:32] ← 0;
				FI;
			FI;
		*/

		auto val = pkt->insert_csel(
			pkt->insert_read_reg(value_type::u1(), REGOFF_SF)->val(), read_operand(pkt, xed_inst, 0)->val(), read_operand(pkt, xed_inst, 1)->val());
		write_operand(pkt, xed_inst, 0, val->val());

		break;
	}

	case XED_ICLASS_CMOVNZ: {
		// Not Zero: ZF==0

		auto val = pkt->insert_csel(
			pkt->insert_read_reg(value_type::u1(), REGOFF_ZF)->val(), read_operand(pkt, xed_inst, 0)->val(), read_operand(pkt, xed_inst, 1)->val());
		write_operand(pkt, xed_inst, 0, val->val());

		break;
	}

	case XED_ICLASS_CMOVZ: {
		// Zero: ZF==1

		auto val = pkt->insert_csel(
			pkt->insert_read_reg(value_type::u1(), REGOFF_ZF)->val(), read_operand(pkt, xed_inst, 1)->val(), read_operand(pkt, xed_inst, 0)->val());
		write_operand(pkt, xed_inst, 0, val->val());

		break;
	}

	case XED_ICLASS_CMOVNLE: {
		// Not Less or Equal: ZF==0, SF==OF (!ZF & SF==OF)

		auto zf = pkt->insert_read_reg(value_type::u1(), REGOFF_ZF);
		auto sf = pkt->insert_read_reg(value_type::u1(), REGOFF_SF);
		auto of = pkt->insert_read_reg(value_type::u1(), REGOFF_OF);

		auto cond = pkt->insert_and(pkt->insert_not(zf->val())->val(), pkt->insert_cmpeq(sf->val(), of->val())->val());

		auto val = pkt->insert_csel(cond->val(), read_operand(pkt, xed_inst, 1)->val(), read_operand(pkt, xed_inst, 0)->val());
		write_operand(pkt, xed_inst, 0, val->val());

		break;
	}

	case XED_ICLASS_CQO: {
		// TODO: Operand sizes
		auto sign_set = pkt->insert_read_reg(value_type::u64(), reg_to_offset(XED_REG_RAX));
		sign_set = pkt->insert_trunc(value_type::u1(), pkt->insert_asr(sign_set->val(), pkt->insert_constant_u32(63)->val())->val());

		auto sx = pkt->insert_csel(sign_set->val(), pkt->insert_constant_u64(0xffffffffffffffffull)->val(), pkt->insert_constant_u64(0)->val());
		pkt->insert_write_reg(reg_to_offset(XED_REG_RDX), sx->val());
		break;
	}

	case XED_ICLASS_SAR: {
		auto src = read_operand(pkt, xed_inst, 0);
		auto amt = pkt->insert_constant_u32(xed_decoded_inst_get_unsigned_immediate(xed_inst));
		write_operand(pkt, xed_inst, 0, pkt->insert_asr(src->val(), amt->val())->val());
		break;
	}

	case XED_ICLASS_SHR: {
		auto src = read_operand(pkt, xed_inst, 0);
		auto amt = pkt->insert_constant_u32(xed_decoded_inst_get_unsigned_immediate(xed_inst));
		write_operand(pkt, xed_inst, 0, pkt->insert_lsr(src->val(), amt->val())->val());
		break;
	}

	case XED_ICLASS_SHL: {
		auto src = read_operand(pkt, xed_inst, 0);
		auto amt = pkt->insert_constant_u32(xed_decoded_inst_get_unsigned_immediate(xed_inst));
		write_operand(pkt, xed_inst, 0, pkt->insert_lsl(src->val(), amt->val())->val());
		break;
	}

	case XED_ICLASS_NOP:
	case XED_ICLASS_HLT:
	case XED_ICLASS_CPUID:
	case XED_ICLASS_SYSCALL:
	case XED_ICLASS_PAND:
		break;

	default:
		return nullptr; // throw std::runtime_error("unsupported instruction");
	}

	pkt->insert_end();

	return pkt;
}

std::shared_ptr<chunk> x86_input_arch::translate_chunk(off_t base_address, const void *code, size_t code_size)
{
	auto c = std::make_shared<chunk>();

	initialise_xed();

	const uint8_t *mc = (const uint8_t *)code;

	std::cerr << "chunk @ " << base_address << std::endl;

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

		auto i = translate_instruction(base_address, &xedd);
		if (!i) {
			break;
		}

		c->add_packet(i);

		offset += length;
		base_address += length;
	} while (offset < code_size);

	return c;
}
