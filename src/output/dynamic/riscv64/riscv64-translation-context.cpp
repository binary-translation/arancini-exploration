#include <arancini/ir/node.h>
#include <arancini/output/dynamic/chain.h>
#include <arancini/output/dynamic/riscv64/arithmetic.h>
#include <arancini/output/dynamic/riscv64/bitwise.h>
#include <arancini/output/dynamic/riscv64/encoder/riscv64-constants.h>
#include <arancini/output/dynamic/riscv64/flags.h>
#include <arancini/output/dynamic/riscv64/register_usage_identifier.h>
#include <arancini/output/dynamic/riscv64/riscv64-translation-context.h>
#include <arancini/output/dynamic/riscv64/shift.h>
#include <arancini/output/dynamic/riscv64/utils.h>
#include <arancini/runtime/exec/x86/x86-cpu-state.h>

#include <unordered_map>

using namespace arancini::output::dynamic::riscv64;
using namespace arancini::ir;

/**
 * Translations assumes FP holds pointer to CPU state.
 * Flags are always stored in registers S8 (ZF), S9 (CF), S10 (OF), S11(SF).
 * Memory accesses need to use MEM_BASE.
 */

#define X86_OFFSET_OF(reg) __builtin_offsetof(struct arancini::runtime::exec::x86::x86_cpu_state, reg)
enum class reg_offsets : unsigned long {
#define DEFREG(ctype, ltype, name) name = X86_OFFSET_OF(name),
#include <arancini/input/x86/reg.def>
#undef DEFREG
};

static std::unordered_map<unsigned long, Register> flag_map {
	{ (unsigned long)reg_offsets::ZF, ZF },
	{ (unsigned long)reg_offsets::CF, CF },
	{ (unsigned long)reg_offsets::OF, OF },
	{ (unsigned long)reg_offsets::SF, SF },
};

Register riscv64_translation_context::next_register()
{
	constexpr static Register registers[] { S1, A0, A1, A2, A3, A4, A5, T0, T1, T2, A6, A7, S2, S3, S4, S5, S6, S7, T3, T4, T5, GP };

	if (reg_allocator_index_ >= std::size(registers)) {
		throw std::runtime_error("RISC-V DBT ran out of registers for packet at " + std::to_string(current_address_));
	}
	while (reg_used_[registers[reg_allocator_index_++].encoding()])
		;
	return registers[reg_allocator_index_ - 1];
}

/**
 * Used to get the register for the given port.
 * Will return the previously allocated TypedRegister or a new one with type set accordingly to the given port.
 * @param p Port of the register. Can be null for temporary.
 * @param reg1 Optional. Use this register for the lower half instead of allocating a new one.
 * @param reg2 Optional. Use this register for the upper half instead of allocating a new one.
 * @return A pair reference to the allocated Register and a boolean indicating whether the allocating was new.
 */
std::pair<TypedRegister &, bool> riscv64_translation_context::allocate_register(const port *p, std::optional<Register> reg1, std::optional<Register> reg2)
{
	if (p && reg_for_port_.count(p)) {
		return { reg_for_port_.at(p), false };
	}
	if (!p) {
		Register r1 = reg1 ? *reg1 : next_register();
		temporaries.emplace_front(r1);
		return { temporaries.front(), true };
	}

	switch (p->type().width()) {
	case 512: // FIXME proper
	case 128: {
		Register r1 = reg1 ? *reg1 : next_register();
		Register r2 = reg2 ? *reg2 : next_register();
		auto [a, b] = reg_for_port_.emplace(std::piecewise_construct, std::forward_as_tuple(p), std::forward_as_tuple(r1, r2));
		TypedRegister &tr = a->second;
		tr.set_type(p->type());
		return { tr, true };
	}
	case 64:
	case 32:
	case 16:
	case 8:
	case 1: {
		Register r1 = reg1 ? *reg1 : next_register();
		auto [a, b] = reg_for_port_.emplace(p, r1);
		TypedRegister &tr = a->second;
		tr.set_type(p->type());
		return { tr, true };
	}
	default:
		throw std::runtime_error("Invalid type for register allocation");
	}
}

Register riscv64_translation_context::get_or_assign_mapped_register(unsigned long idx)
{
	unsigned int &i = reg_map_[idx - 1];
	if (!i) {
		Register reg = next_register();
		i = reg.encoding();
		reg_used_[i] = true;
	}
	return Register { i };
}

Register riscv64_translation_context::get_or_load_mapped_register(unsigned long idx)
{
	unsigned int &i = reg_map_[idx - 1];
	if (!i) {
		Register reg = next_register();
		i = reg.encoding();
		reg_used_[i] = true;
		assembler_.ld(reg, { FP, static_cast<intptr_t>(8 * idx) }); // FIXME hardcoded
	}
	return Register { i };
}

void riscv64_translation_context::write_back_registers()
{
	for (size_t i = 0; i < reg_map_.size(); ++i) {
		if (reg_map_[i]) {
			assembler_.sd(Register { reg_map_[i] }, { FP, static_cast<intptr_t>(8 * i + 8) }); // FIXME hardcoded offset
		}
	}
}

bool insert_ebreak = false;

/**
 * Adds a NOP with a non standard encoding to the generated instructions as a marker.
 * @param payload The immediate to use in the immediate field of the NOP
 */
void riscv64_translation_context::add_marker(int payload)
{
	// TODO Remove/only in debug
	assembler_.li(ZERO, payload);
}

void riscv64_translation_context::begin_block()
{
	// TODO Remove/only in debug
	if (insert_ebreak) {
		assembler_.ebreak();
	}
	reg_map_.fill(0);
	reg_used_.reset();

	add_marker(1);
}

void riscv64_translation_context::begin_instruction(off_t address, const std::string &disasm)
{
	add_marker(2);
	reg_allocator_index_ = 0;
	reg_for_port_.clear();
	temporaries.clear();
	locals_.clear();
	labels_.clear();
	nodes_.clear();
	current_address_ = address;
	ret_val_ = 0;

	// Enable automatic breakpoints after certain program counter
//	if (address == 0x401cb3) {
//		insert_ebreak = true;
//		assembler_.ebreak();
//	}

}

void riscv64_translation_context::end_instruction()
{
	register_used_visitor v;
	for (auto &item : nodes_) {
		item->accept(v);
	}

	for (size_t i = 0; i < std::size(v.reg_used_); ++i) {
		if (v.reg_used_[i]) {
			get_or_load_mapped_register(i + 1);
		}
	}

	for (const auto &item : nodes_) {
		materialise(item);
	}
	add_marker(-2);
}

void riscv64_translation_context::end_block()
{
	// TODO Remove/only in debug
	if (insert_ebreak) {
		assembler_.ebreak();
	}

	assembler_.li(A0, ret_val_);
	assembler_.ret();
}

void riscv64_translation_context::lower(ir::node *n)
{
	// Defer until end of block (when generation is finished)
	nodes_.push_back(n);
}

using load_store_func_t = decltype(&Assembler::ld);

static const std::unordered_map<std::size_t, load_store_func_t> load_instructions {
	{ 1, &Assembler::lb },
	{ 8, &Assembler::lb },
	{ 16, &Assembler::lh },
	{ 32, &Assembler::lw },
	{ 64, &Assembler::ld },
};

static const std::unordered_map<std::size_t, load_store_func_t> store_instructions {
	{ 1, &Assembler::sb },
	{ 8, &Assembler::sb },
	{ 16, &Assembler::sh },
	{ 32, &Assembler::sw },
	{ 64, &Assembler::sd },
};

std::optional<int64_t> riscv64_translation_context::get_as_int(const node *n)
{
	switch (n->kind()) {
	case node_kinds::constant: {
		const constant_node &node = *reinterpret_cast<const constant_node *>(n);
		if (!is_gpr_or_flag(node.val())) {
			throw std::runtime_error("unsupported width on constant");
		}
		return node.const_val_i();
	} break;
	case node_kinds::binary_arith: {
		const auto &node = *reinterpret_cast<const binary_arith_node *>(n);
		bool z_needed = !node.zero().targets().empty();
		bool v_needed = !node.overflow().targets().empty();
		bool c_needed = !node.carry().targets().empty();
		bool n_needed = !node.negative().targets().empty();
		bool flags_needed = z_needed || v_needed || c_needed || n_needed;
		if (!flags_needed) {
			const std::optional<int64_t> &lhs = get_as_int(node.lhs().owner());
			if (lhs) {
				const std::optional<int64_t> &rhs = get_as_int(node.rhs().owner());
				if (rhs) {
					switch (node.op()) {
					case binary_arith_op::add:
						return *lhs + *rhs;
					case binary_arith_op::sub:
						return *lhs - *rhs;
					case binary_arith_op::mul:
						return *lhs * *rhs;
					case binary_arith_op::div:
						return *lhs / *rhs;
					case binary_arith_op::mod:
						return *lhs % *rhs;
					case binary_arith_op::band:
						return *lhs & *rhs;
					case binary_arith_op::bor:
						return *lhs | *rhs;
					case binary_arith_op::bxor:
						return *lhs ^ *rhs;
					}
				}
			}
		}
	} break;
	case node_kinds::cast: {
		const auto &node = *reinterpret_cast<const cast_node *>(n);
		const std::optional<int64_t> &src_val = get_as_int(node.source_value().owner());
		if (src_val) {
			int width = node.source_value().type().element_width();
			switch (node.op()) {
			case cast_op::bitcast:
				return src_val;
			case cast_op::trunc:
			case cast_op::zx:
				return (((uint64_t)*src_val) << (64 - width)) >> (64 - width);
			case cast_op::sx:
				return (*src_val << (64 - width)) >> (64 - width);
			}
		}
	} break;
	case node_kinds::read_pc:
		return current_address_;
	case node_kinds::bit_extract: {
		const auto &node = *reinterpret_cast<const bit_extract_node *>(n);
		const std::optional<int64_t> &src_val = get_as_int(node.source_value().owner());
		if (src_val) {
			int from = node.from();
			int length = node.length();
			return (((uint64_t)*src_val) << (64 - (from + length))) >> (64 - (length));
		}
	} break;
	case node_kinds::bit_shift: {
		const auto &node = *reinterpret_cast<const bit_shift_node *>(n);
		const std::optional<int64_t> &src_val = get_as_int(node.input().owner());
		const std::optional<int64_t> &amt = get_as_int(node.amount().owner());
		if (src_val && amt) {
			switch (node.op()) {
			case shift_op::lsl:
				return *src_val << *amt;
			case shift_op::lsr:
				return ((uint64_t)*src_val) >> *amt;
			case shift_op::asr:
				return *src_val >> *amt;
			}
		}
	} break;
	}
	return std::nullopt;
}

std::optional<std::reference_wrapper<TypedRegister>> riscv64_translation_context::materialise(const node *n)
{
	if (!n) {
		throw std::runtime_error("RISC-V DBT received NULL pointer to node");
	}

	const std::optional<int64_t> &i = get_as_int(n);
	if (i) {
		return materialise_constant(*i);
	}

	switch (n->kind()) {
	case node_kinds::bit_shift:
		return materialise_bit_shift(*reinterpret_cast<const bit_shift_node *>(n));
	case node_kinds::read_reg:
		return materialise_read_reg(*reinterpret_cast<const read_reg_node *>(n));
	case node_kinds::write_reg:
		materialise_write_reg(*reinterpret_cast<const write_reg_node *>(n));
		return std::nullopt;
	case node_kinds::read_mem:
		return materialise_read_mem(*reinterpret_cast<const read_mem_node *>(n));
	case node_kinds::write_mem:
		materialise_write_mem(*reinterpret_cast<const write_mem_node *>(n));
		return std::nullopt;
	case node_kinds::read_pc:
		return materialise_read_pc(*reinterpret_cast<const read_pc_node *>(n));
	case node_kinds::write_pc:
		materialise_write_pc(*reinterpret_cast<const write_pc_node *>(n));
		return std::nullopt;
	case node_kinds::label:
		materialise_label(*reinterpret_cast<const label_node *>(n));
		return std::nullopt;
	case node_kinds::br:
		materialise_br(*reinterpret_cast<const br_node *>(n));
		return std::nullopt;
	case node_kinds::cond_br:
		materialise_cond_br(*reinterpret_cast<const cond_br_node *>(n));
		return std::nullopt;
	case node_kinds::binary_arith:
		return materialise_binary_arith(*reinterpret_cast<const binary_arith_node *>(n));
	case node_kinds::unary_arith:
		return materialise_unary_arith(*reinterpret_cast<const unary_arith_node *>(n));
	case node_kinds::ternary_arith:
		return materialise_ternary_arith(*reinterpret_cast<const ternary_arith_node *>(n));
	case node_kinds::bit_extract:
		return materialise_bit_extract(*reinterpret_cast<const bit_extract_node *>(n));
	case node_kinds::bit_insert:
		return materialise_bit_insert(*reinterpret_cast<const bit_insert_node *>(n));
	case node_kinds::cast:
		return materialise_cast(*reinterpret_cast<const cast_node *>(n));
	case node_kinds::binary_atomic:
		return materialise_binary_atomic(*reinterpret_cast<const binary_atomic_node *>(n));
	case node_kinds::ternary_atomic:
		return materialise_ternary_atomic(*reinterpret_cast<const ternary_atomic_node *>(n));
	case node_kinds::csel:
		return materialise_csel(*reinterpret_cast<const csel_node *>(n));
	case node_kinds::internal_call:
		materialise_internal_call(*reinterpret_cast<const internal_call_node *>(n));
		return std::nullopt;
	case node_kinds::vector_insert:
		return materialise_vector_insert(*reinterpret_cast<const vector_insert_node *>(n));
	case node_kinds::vector_extract:
		return materialise_vector_extract(*reinterpret_cast<const vector_extract_node *>(n));
	case node_kinds::read_local:
		return locals_.at(reinterpret_cast<const read_local_node *>(n)->local());
	case node_kinds::write_local: {
		auto &node = *reinterpret_cast<const write_local_node *>(n);
		TypedRegister &write_reg = *(materialise(node.write_value().owner()));
		uint32_t reg_enc;
		if (!locals_.count(node.local())) {
			TypedRegister &local = allocate_register().first;
			locals_.emplace(node.local(), std::ref(local));
			reg_enc = local.encoding();
		} else {
			reg_enc = locals_.at(node.local()).get().encoding();
		}
		assembler_.mv(Register { reg_enc }, write_reg);
		return std::nullopt;
	}
	default:
		throw std::runtime_error("unsupported node");
	}
}

TypedRegister &riscv64_translation_context::materialise_ternary_atomic(const ternary_atomic_node &n)
{

	if (!(is_gpr(n.val()) && is_gpr(n.rhs()) && is_gpr(n.address()) && is_gpr(n.top()))) {
		throw std::runtime_error("Unsupported type for Ternary Atomic");
	}

	TypedRegister &dstAddr = *materialise(n.address().owner());
	TypedRegister &acc = *materialise(n.rhs().owner());
	TypedRegister &src = *materialise(n.top().owner());
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}
	Label fail;
	Label retry;
	Label end;

	auto [reg, _] = allocate_register();

	assembler_.add(reg, dstAddr, MEM_BASE);

	bool z_needed = !n.zero().targets().empty();
	bool v_needed = !n.overflow().targets().empty();
	bool c_needed = !n.carry().targets().empty();
	bool n_needed = !n.negative().targets().empty();
	bool flags_needed = z_needed || v_needed || c_needed || n_needed;

	using load_reserve_type = decltype(&Assembler::lrd);
	using store_conditional_type = decltype(&Assembler::scd);

	load_reserve_type lr = n.val().type().element_width() == 32 ? &Assembler::lrw : &Assembler::lrd;
	store_conditional_type sc = n.val().type().element_width() == 32 ? &Assembler::scw : &Assembler::scd;
	load_store_func_t store = n.val().type().element_width() == 32 ? &Assembler::sw : &Assembler::sd;

	auto addr = Address { reg };
	// FIXME Correct memory ordering?
	switch (n.op()) {

	case ternary_atomic_op::cmpxchg:
		switch (n.rhs().type().element_width()) {
		case 64:
		case 32: {
			assembler_.Bind(&retry);

			(assembler_.*lr)(out_reg, addr, std::memory_order_acq_rel);
			assembler_.bne(out_reg, acc, &fail, Assembler::kNearJump);
			(assembler_.*sc)(out_reg, src, addr, std::memory_order_acq_rel);
			assembler_.bnez(out_reg, &retry, Assembler::kNearJump);

			// Flags from comparison matching (i.e subtraction of equal values)
			if (z_needed) {
				assembler_.li(ZF, 1);
			}
			if (c_needed) {
				assembler_.li(CF, 0);
			}
			if (v_needed) {
				assembler_.li(OF, 0);
			}
			if (n_needed) {
				assembler_.li(SF, 0);
			}

			assembler_.j(&end, Assembler::kNearJump);

			assembler_.Bind(&fail);

			if (flags_needed) {
				TypedRegister temp_reg { SF };
				temp_reg.set_type(n.val().type());
				sub(assembler_, temp_reg, acc, out_reg);
				sub_flags(assembler_, temp_reg, acc, out_reg, z_needed, v_needed, c_needed, n_needed);
			}

			// Write back updated acc value
			assembler_.mv(get_or_assign_mapped_register(reinterpret_cast<read_reg_node *>(n.rhs().owner())->regidx()), reg);

			assembler_.Bind(&end);
		} break;
		default:
			throw std::runtime_error("unsupported cmpxchg width");
		}
		return out_reg;
	default:
		throw std::runtime_error("unsupported ternary atomic operation");
	}
}

std::optional<std::reference_wrapper<TypedRegister>> riscv64_translation_context::materialise_binary_atomic(const binary_atomic_node &n)
{

	if (!(is_gpr(n.val()) && is_gpr(n.rhs()) && is_gpr(n.address()))) {
		throw std::runtime_error("Unsupported types for binary atomic");
	}

	TypedRegister &dstAddr = *materialise(n.address().owner());
	TypedRegister &src = *materialise(n.rhs().owner());
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}

	bool z_needed = !n.zero().targets().empty();
	bool v_needed = !n.overflow().targets().empty();
	bool c_needed = !n.carry().targets().empty();
	bool n_needed = !n.negative().targets().empty();
	bool flags_needed = z_needed || v_needed || c_needed || n_needed;

	auto [reg, _] = allocate_register();

	assembler_.add(reg, dstAddr, MEM_BASE);

	auto addr = Address { reg };
	// FIXME Correct memory ordering?
	switch (n.op()) {
	case binary_atomic_op::xadd:
	case binary_atomic_op::add: {
		switch (n.val().type().element_width()) {
		case 64:
			assembler_.amoaddd(out_reg, src, addr, std::memory_order_acq_rel);
			break;
		case 32:
			assembler_.amoaddw(out_reg, src, addr, std::memory_order_acq_rel);
			break;
		default:
			throw std::runtime_error("unsupported xadd width");
		}

		if (flags_needed) {
			TypedRegister temp_result_reg { SF };
			temp_result_reg.set_type(n.val().type());

			add(assembler_, temp_result_reg, out_reg, src); // Actual sum for flag generation
			add_flags(assembler_, temp_result_reg, out_reg, src, z_needed, v_needed, c_needed, n_needed);
		}

		if (n.op() == binary_atomic_op::xadd) {
			switch (n.val().type().element_width()) { // FIXME This feels so wrong
			case 32:
				assembler_.slli(out_reg, out_reg, 32);
				assembler_.srli(out_reg, out_reg, 32);
				[[fallthrough]];
			case 64:
				assembler_.mv(get_or_assign_mapped_register(reinterpret_cast<read_reg_node *>(n.rhs().owner())->regidx()), reg);
				break;
			default:
				throw std::runtime_error("unsupported xadd width");
			}
			return out_reg;
		}
		return std::nullopt;
	}
	case binary_atomic_op::sub:
		assembler_.neg(out_reg, src);
		switch (n.val().type().element_width()) {
		case 64:
			assembler_.amoaddd(out_reg, out_reg, addr, std::memory_order_acq_rel);
			break;
		case 32:
			assembler_.amoaddw(out_reg, src, addr, std::memory_order_acq_rel);
			break;
		default:
			throw std::runtime_error("unsupported lock sub width");
		}

		if (flags_needed) {
			TypedRegister temp_result_reg { SF };
			temp_result_reg.set_type(n.val().type());

			sub(assembler_, temp_result_reg, out_reg, src); // Actual difference for flag generation
			sub_flags(assembler_, temp_result_reg, out_reg, src, z_needed, v_needed, c_needed, n_needed);
		}
		return std::nullopt;
	case binary_atomic_op::band: {
		switch (n.val().type().element_width()) {
		case 64:
			assembler_.amoandd(out_reg, src, addr, std::memory_order_acq_rel);
			if (flags_needed) {
				assembler_.and_(SF, out_reg, src); // Actual and for flag generation
			}
			break;
		case 32:
			assembler_.amoandw(out_reg, src, addr, std::memory_order_acq_rel);
			if (flags_needed) {
				assembler_.and_(SF, out_reg, src); // Actual and for flag generation
				assembler_.slli(SF, SF, 32); // Get rid of higher 32 bits
			}
			break;
		default:
			throw std::runtime_error("unsupported lock and width");
		}

		TypedRegister out = TypedRegister { SF };
		zero_sign_flag(assembler_, out, z_needed, n_needed);
		return std::nullopt;
	}
	case binary_atomic_op::bor: {
		switch (n.val().type().element_width()) {
		case 64:
			assembler_.amoord(out_reg, src, addr, std::memory_order_acq_rel);
			if (flags_needed) {
				assembler_.or_(SF, out_reg, src); // Actual or for flag generation
			}
			break;
		case 32:
			assembler_.amoorw(out_reg, src, addr, std::memory_order_acq_rel);
			if (flags_needed) {
				assembler_.or_(SF, out_reg, src); // Actual or for flag generation
				assembler_.slli(SF, SF, 32); // Get rid of higher 32 bits
			}
			break;
		default:
			throw std::runtime_error("unsupported lock or width");
		}

		TypedRegister out = TypedRegister { SF };
		zero_sign_flag(assembler_, out, z_needed, n_needed);
		return std::nullopt;
	}
	case binary_atomic_op::bxor: {
		switch (n.val().type().element_width()) {
		case 64:
			assembler_.amoxord(out_reg, src, addr, std::memory_order_acq_rel);
			if (flags_needed) {
				assembler_.xor_(SF, out_reg, src); // Actual xor for flag generation
			}
			break;
		case 32:
			assembler_.amoxorw(out_reg, src, addr, std::memory_order_acq_rel);

			if (flags_needed) {
				assembler_.xor_(SF, out_reg, src); // Actual xor for flag generation
				assembler_.slli(SF, SF, 32); // Get rid of higher 32 bits
			}
			break;
		default:
			throw std::runtime_error("unsupported lock xor width");
		}

		TypedRegister out = TypedRegister { SF };
		zero_sign_flag(assembler_, out, z_needed, n_needed);
		return std::nullopt;
	}
	case binary_atomic_op::xchg:
		switch (n.val().type().element_width()) {
		case 64:
			assembler_.amoswapd(out_reg, src, addr, std::memory_order_acq_rel);
			break;
		case 32:
			assembler_.amoswapw(out_reg, src, addr, std::memory_order_acq_rel);
			break;
		default:
			throw std::runtime_error("unsupported xchg width");
		}

		switch (n.val().type().element_width()) {
		case 32:
			assembler_.slli(out_reg, out_reg, 32);
			assembler_.srli(out_reg, out_reg, 32);
			[[fallthrough]];
		case 64:
			assembler_.mv(get_or_assign_mapped_register(reinterpret_cast<read_reg_node *>(n.rhs().owner())->regidx()), reg);
			break;
		default:
			throw std::runtime_error("unsupported xchg width");
		}
		return out_reg;
	default:
		throw std::runtime_error("unsupported binary atomic operation");
	}
}

TypedRegister &riscv64_translation_context::materialise_cast(const cast_node &n)
{
	TypedRegister &src_reg = *materialise(n.source_value().owner());

	bool works = (is_scalar_int(n.val())
					 || ((is_int_vector(n.val(), 2, 64) || is_int_vector(n.val(), 4, 32) || is_int_vector(n.val(), 4, 128)) && n.op() == cast_op::bitcast))
		&& (is_gpr_or_flag(n.source_value())
			|| ((is_i128(n.source_value()) || is_int(n.source_value(), 512) || is_int_vector(n.source_value(), 4, 32) || is_int_vector(n.source_value(), 2, 64))
				&& (n.op() == cast_op::trunc || n.op() == cast_op::bitcast)));
	if (!works) {
		throw std::runtime_error("unsupported types on cast operation");
	}

	switch (n.op()) {

	case cast_op::bitcast:
		// Assumes types are compatible
		// Just noop for now as all supported types use same layouts for same width
		return src_reg;
	case cast_op::sx:
		if (is_i128(n.val())) {
			// No true 128bit sign extension, just assume upper 64 unused
			for (const node *target : n.val().targets()) {
				if (target->kind() != node_kinds::binary_arith) {
					throw std::runtime_error("unsupported types on cast sx operation");
				}
				binary_arith_op op = ((const binary_arith_node *)target)->op();
				if (!(op == binary_arith_op::mul || op == binary_arith_op::div || op == binary_arith_op::mod)) {
					throw std::runtime_error("unsupported types on cast sx operation");
				}
			}
			fixup(assembler_, src_reg, src_reg, value_type::s64());
		}
		fixup(assembler_, src_reg, src_reg, n.val().type().get_signed_type());
		return src_reg;

	case cast_op::zx: {
		if (n.val().type().element_width() == 128) {
			if (n.source_value().type().element_width() == 64) {
				auto [out_reg, valid] = allocate_register(&n.val(), src_reg, ZERO);
				return out_reg;
			} else {
				auto [out_reg, valid] = allocate_register(&n.val(), std::nullopt, ZERO);
				if (!valid) {
					return out_reg;
				}
				fixup(assembler_, out_reg, src_reg, n.val().type().get_unsigned_type());
				return out_reg;
			}
		}
		if (is_flag(n.source_value())) { // Flags always zero extended
			return src_reg;
		}

		auto [out_reg, valid] = allocate_register(&n.val());
		if (!valid) {
			return out_reg;
		}
		fixup(assembler_, out_reg, src_reg, n.val().type().get_unsigned_type());
		return out_reg;
	}
	case cast_op::trunc: {
		if (is_i128(n.val())) {
			throw std::runtime_error("truncate to 128bit unsupported");
		}

		if (n.val().type().element_width() == 64) {
			if (src_reg.type().element_width() > 64) {
				return allocate_register(&n.val(), src_reg.reg1()).first;
			}
			return src_reg;
		}

		auto [out_reg, valid] = allocate_register(&n.val());
		if (!valid) {
			return out_reg;
		}

		truncate(assembler_, out_reg, src_reg);
		return out_reg;
	}
	default:
		throw std::runtime_error("unsupported cast op");
	}
}

TypedRegister &riscv64_translation_context::materialise_bit_extract(const bit_extract_node &n)
{
	int from = n.from();
	int length = n.length();
	TypedRegister &src = *materialise(n.source_value().owner());

	if (n.source_value().type().element_width() == 128) {
		if (length == 64) { // One half replaced by input
			if (from == 0) {
				return allocate_register(&n.val(), src.reg1()).first;
			} else if (from == 64) {
				return allocate_register(&n.val(), src.reg2()).first;
			}
		}
	}

	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}

	bit_extract(assembler_, out_reg, src, from, length);

	return out_reg;
}

TypedRegister &riscv64_translation_context::materialise_bit_insert(const bit_insert_node &n)
{
	int to = n.to();
	int length = n.length();

	TypedRegister &src = *materialise(n.source_value().owner());

	TypedRegister &bits = *materialise(n.bits().owner());

	if (is_i128(n.val()) && is_i128(n.source_value()) && is_gpr(n.bits())) {
		if (to == 64 && length == 64) {
			// Only map don't modify registers
			auto [out_reg, _] = allocate_register(&n.val(), src.reg1(), bits);
			return out_reg;
		} else if (to + length <= 64) {
			// Map upper and insert into lower
			auto [out_reg, valid] = allocate_register(&n.val(), std::nullopt, src.reg2());
			if (valid) {
				bit_insert(assembler_, out_reg, src, bits, to, length, allocate_register(nullptr).first);
			}
			return out_reg;
		} else if (to >= 64) {
			// Map lower and insert into upper
			auto [out_reg, valid] = allocate_register(&n.val(), src.reg1());
			if (valid) {
				bit_insert(assembler_, out_reg, src, bits, to, length, allocate_register(nullptr).first);
			}
			return out_reg;
		} else {
			throw std::runtime_error("Unsupported bit insert arguments for 128 bit width.");
		}
	} else if (is_gpr(n.val()) && is_gpr(n.source_value()) && is_gpr(n.bits())) {
		// Insert into 64B register
		auto [out_reg, valid] = allocate_register(&n.val());
		if (valid) {
			bit_insert(assembler_, out_reg, src, bits, to, length, allocate_register(nullptr).first);
		}
		return out_reg;
	} else {
		throw std::runtime_error("Unsupported bit insert width.");
	}
}

TypedRegister &riscv64_translation_context::materialise_read_reg(const read_reg_node &n)
{
	const port &value = n.val();
	if (is_int(value, 64) && value.targets().size() == 1 && n.regoff() <= static_cast<unsigned long>(reg_offsets::R15)) { // 64bit GPR only used once
		unsigned int &i = reg_map_[n.regidx() - 1];
		return allocate_register(&n.val(), Register { i }).first;
	}

	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}
	if (is_gpr(value)) {
		if (n.regoff() > static_cast<unsigned long>(reg_offsets::R15)) { // Not GPR
			auto load_instr = load_instructions.at(value.type().element_width());
			(assembler_.*load_instr)(out_reg, { FP, static_cast<intptr_t>(n.regoff()) });
			out_reg.set_actual_width();
			out_reg.set_type(value_type::u64());
		} else {
			Register reg = get_or_load_mapped_register(n.regidx());
			if (is_int(value, 32)) {
				assembler_.sextw(out_reg, reg);
				out_reg.set_actual_width();
				out_reg.set_type(value_type::u64());
			} else {
				assembler_.mv(out_reg, reg);
			}
		}

		return out_reg;
	}
	if (is_flag(value)) {
		assembler_.lb(out_reg, { FP, static_cast<intptr_t>(n.regoff()) });
		out_reg.set_type(value_type::u64());
		return out_reg;
	} else if (is_i128(value) || is_int(value, 512)) {
		assembler_.ld(out_reg.reg1(), { FP, static_cast<intptr_t>(n.regoff()) });
		assembler_.ld(out_reg.reg2(), { FP, static_cast<intptr_t>(n.regoff() + 8) });
		out_reg.set_type(value_type::u128());
		return out_reg;
	}

	throw std::runtime_error("Unsupported width on register read: " + std::to_string(value.type().width()));
}

void riscv64_translation_context::materialise_write_reg(const write_reg_node &n)
{
	const port &value = n.value();
	if (is_gpr(value)) {
		TypedRegister &reg = *(materialise(value.owner()));
		if (n.regoff() > static_cast<unsigned long>(reg_offsets::R15)) { // Not GPR
			auto store_instr = store_instructions.at(value.type().element_width());
			(assembler_.*store_instr)(reg, { FP, static_cast<intptr_t>(n.regoff()) });
		}
		assembler_.mv(get_or_assign_mapped_register(n.regidx()), reg);
		return;
	} else if (is_flag(value)) {
		Register reg = (!is_flag_port(value)) ? (materialise(value.owner()))->get() : Register { flag_map.at(n.regoff()) };
		if (is_flag_port(value) && !reg_for_port_.count(&value.owner()->val())) {
			// Result of node not written only flags needed
			materialise(value.owner());
		}
		assembler_.sb(reg, { FP, static_cast<intptr_t>(n.regoff()) });
		return;
	} else if (is_i128(value) || is_int_vector(value, 2, 64) || is_int_vector(value, 4, 32) || is_int(value, 512) || is_int_vector(value, 4, 128)) {
		// Treat 512 as 128 for now. Assuming it is just 128 bit instructions acting on 512 registers
		TypedRegister &reg = *(materialise(value.owner())); // Will give lower 64bit

		assembler_.sd(reg.reg1(), { FP, static_cast<intptr_t>(n.regoff()) });
		assembler_.sd(reg.reg2(), { FP, static_cast<intptr_t>(n.regoff() + 8) });
		return;
	}

	throw std::runtime_error("Unsupported width on register write: " + std::to_string(value.type().width()));
}

TypedRegister &riscv64_translation_context::materialise_read_mem(const read_mem_node &n)
{
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}
	TypedRegister &addr_reg = *(materialise(n.address().owner())); // FIXME Assumes address has 64bit size in IR

	auto [reg, _] = allocate_register();

	assembler_.add(reg, addr_reg, MEM_BASE);

	Address addr { reg };

	if (is_i128(n.val())) {
		assembler_.ld(out_reg.reg1(), addr);
		assembler_.ld(out_reg.reg2(), Address { reg, 8 });
		return out_reg;
	}

	if (!(is_gpr(n.val()) && is_gpr(n.address()))) {
		throw std::runtime_error("unsupported width on read mem operation");
	}

	auto load_instr = load_instructions.at(n.val().type().element_width());
	(assembler_.*load_instr)(out_reg, addr);
	out_reg.set_actual_width();
	out_reg.set_type(value_type::u64());

	return out_reg;
}

void riscv64_translation_context::materialise_write_mem(const write_mem_node &n)
{
	TypedRegister &src_reg = *materialise(n.value().owner());
	TypedRegister &addr_reg = *materialise(n.address().owner());

	auto [reg, _] = allocate_register(); // Temporary

	assembler_.add(reg, addr_reg, MEM_BASE); // FIXME Assumes address has 64bit size in IR

	Address addr { reg };

	if (is_i128(n.value())) {
		assembler_.sd(src_reg.reg1(), addr);
		assembler_.sd(src_reg.reg2(), Address { reg, 8 });
		return;
	}

	if (!(is_gpr(n.value()) && is_gpr(n.address()))) {
		throw std::runtime_error("unsupported width on write mem operation");
	}

	auto store_instr = store_instructions.at(n.value().type().element_width());
	(assembler_.*store_instr)(src_reg, addr);
}

TypedRegister &riscv64_translation_context::materialise_read_pc(const read_pc_node &n) { return materialise_constant(current_address_); }

void riscv64_translation_context::materialise_write_pc(const write_pc_node &n)
{
	if (!is_gpr(n.value())) {
		throw std::runtime_error("unsupported width on write pc operation");
	}

	if (ret_val_ == 0) { // Only chain on normal block end
		const std::optional<int64_t> &target = get_as_int(n.value().owner());

		if (target) { // Unconditional direct jump or call
			// Set up chain

			// Write back all registers
			write_back_registers();
			// Now A1 register is available

			// Save address to patch jump into when chaining (A1 second ret value)
			assembler_.auipc(A1, 0);

			reg_used_[A1.encoding()] = true; // Force keeping A1 unused
			TypedRegister &reg = materialise_constant(*target);
			assembler_.sd(reg, { FP, static_cast<intptr_t>(reg_offsets::PC) });
			return;
		} else if (n.value().owner()->kind() == node_kinds::csel) {
			const auto &node = *reinterpret_cast<const csel_node *>(n.value().owner());
			const std::optional<int64_t> &target1 = get_as_int(node.falseval().owner());
			const std::optional<int64_t> &target2 = get_as_int(node.trueval().owner());
			if (target1 && target2) { // Conditional direct jump
				TypedRegister &cond = *materialise(node.condition().owner());
				TypedRegister &out = cond != A1 ? cond : allocate_register(nullptr).first;
				extend_to_64(assembler_, out, cond);

				// Set up chain

				// Write back all registers
				write_back_registers();
				// Now A1 register is available

				// Save address to patch jump into when chaining (A1 second ret value)
				assembler_.auipc(A1, 0);

				reg_used_[A1.encoding()] = true; // Force keeping A1 unused
				Label false_calc {}, end {};

				assembler_.beqz(out, &false_calc, Assembler::kNearJump);

				assembler_.auipc(A1, 0);
				TypedRegister &trueval = materialise_constant(*target2);
				assembler_.sd(trueval, { FP, static_cast<intptr_t>(reg_offsets::PC) });
				assembler_.j(&end, Assembler::kNearJump);

				assembler_.Bind(&false_calc);

				TypedRegister &falseval = materialise_constant(*target1);
				assembler_.sd(falseval, { FP, static_cast<intptr_t>(reg_offsets::PC) });

				assembler_.Bind(&end);
				return;
			}
		}
	}
	// Indirect jump or call or return => No chain
	// FIXME maybe only support 64bit values for PC
	TypedRegister &src_reg = *materialise(n.value().owner());

	Address addr { FP, static_cast<intptr_t>(reg_offsets::PC) };
	auto store_instr = store_instructions.at(n.value().type().element_width());
	(assembler_.*store_instr)(src_reg, addr);

	// Write back all registers
	write_back_registers();

	assembler_.li(A1, 0); // 0 = No chain
}

void riscv64_translation_context::materialise_label(const label_node &n)
{
	auto [it, not_exist] = labels_.try_emplace(&n, nullptr);

	if (not_exist) {
		it->second = std::make_unique<Label>();
	}
	assembler_.Bind(it->second.get());
}

void riscv64_translation_context::materialise_br(const br_node &n)
{
	auto [it, not_exist] = labels_.try_emplace(n.target(), nullptr);

	if (not_exist) {
		it->second = std::make_unique<Label>();
	}
	assembler_.j(it->second.get());
}

void riscv64_translation_context::materialise_cond_br(const cond_br_node &n)
{
	TypedRegister &cond = *materialise(n.cond().owner());

	extend_to_64(assembler_, cond, cond);

	auto [it, not_exist] = labels_.try_emplace(n.target(), nullptr);

	if (not_exist) {
		it->second = std::make_unique<Label>();
	}
	assembler_.bnez(cond, it->second.get());
}

TypedRegister &riscv64_translation_context::materialise_unary_arith(const unary_arith_node &n)
{
	if (!(is_gpr_or_flag(n.val()) && is_gpr_or_flag(n.lhs()))) {
		throw std::runtime_error("unsupported width on unary arith operation");
	}
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}

	TypedRegister &src_reg = *(materialise(n.lhs().owner()));

	if (n.op() == unary_arith_op::bnot) {
		not_(assembler_, out_reg, src_reg);
		return out_reg;
	}

	throw std::runtime_error("unsupported unary arithmetic operation");
}

TypedRegister &riscv64_translation_context::materialise_ternary_arith(const ternary_arith_node &n)
{
	if (!(is_gpr(n.val()) && is_gpr(n.lhs()) && is_gpr(n.rhs()) && is_gpr(n.top()))) {
		throw std::runtime_error("unsupported width on ternary arith operation");
	}
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}

	TypedRegister &src_reg1 = *materialise(n.lhs().owner());
	TypedRegister &src_reg2 = *materialise(n.rhs().owner());
	TypedRegister &src_reg3 = *materialise(n.top().owner());

	bool z_needed = !n.zero().targets().empty();
	bool v_needed = !n.overflow().targets().empty();
	bool c_needed = !n.carry().targets().empty();
	bool n_needed = !n.negative().targets().empty();
	bool flags_needed = z_needed || v_needed || c_needed || n_needed;

	switch (n.op()) {
		// TODO Immediate handling

	case ternary_arith_op::adc:

	{
		TypedRegister temp { CF };
		temp.set_type(n.val().type());
		add(assembler_, temp, src_reg2, src_reg3);
		addi_flags(assembler_, temp, src_reg2, 1, false, v_needed, c_needed, false, SF, ZF);
		add(assembler_, out_reg, src_reg1, temp);
		add_flags(assembler_, out_reg, src_reg1, temp, false, v_needed, c_needed, false);
		if (c_needed) {
			assembler_.or_(CF, CF, ZF); // Total carry out
		}
		if (v_needed) {
			assembler_.xor_(OF, OF, SF); // Total overflow out
		}
		zero_sign_flag(assembler_, out_reg, z_needed, n_needed);
		break;
	}
	case ternary_arith_op::sbb: {
		TypedRegister temp { CF };
		temp.set_type(n.val().type());
		add(assembler_, temp, src_reg2, src_reg3);
		addi_flags(assembler_, temp, src_reg2, 1, false, v_needed, c_needed, false, SF, ZF);
		sub(assembler_, out_reg, src_reg1, temp);
		sub_flags(assembler_, out_reg, src_reg1, temp, false, v_needed, c_needed, false);
		if (c_needed) {
			assembler_.or_(CF, CF, ZF); // Total carry out
		}
		if (v_needed) {
			assembler_.xor_(OF, OF, SF); // Total overflow out
		}
		zero_sign_flag(assembler_, out_reg, z_needed, n_needed);
		break;
	}
	default:
		throw std::runtime_error("unsupported ternary arithmetic operation");
	}
	return out_reg;
}

TypedRegister &riscv64_translation_context::materialise_bit_shift(const bit_shift_node &n)
{
	if (is_i128(n.val()) && is_i128(n.input()) && is_gpr(n.amount())) {
		TypedRegister &src_reg = *materialise(n.input().owner());
		const std::optional<int64_t> &i = get_as_int(n.amount().owner());

		if (i) {
			auto amt = *i;
			if (amt == 64) {
				if (n.op() == shift_op::lsl) {
					return allocate_register(&n.val(), ZERO, src_reg.reg1()).first;
				} else if (n.op() == shift_op::lsr) {
					return allocate_register(&n.val(), src_reg.reg2(), ZERO).first;
				}
			}
		}
	}

	if (!(is_scalar_int(n.val()) && is_scalar_int(n.input()) && is_gpr(n.amount()))) {
		throw std::runtime_error("unsupported width on bit shift operation");
	}
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}

	TypedRegister &src_reg = *materialise(n.input().owner());
	const std::optional<int64_t> &i = get_as_int(n.amount().owner());

	if (i) {
		auto amt = *i;
		if (amt == 0) {
			return src_reg;
		}
		switch (n.op()) {
		case shift_op::lsl:
			slli(assembler_, out_reg, src_reg, amt);
			break;
		case shift_op::lsr:
			srli(assembler_, out_reg, src_reg, amt);
			break;
		case shift_op::asr:
			srai(assembler_, out_reg, src_reg, amt);
			break;
		}
		zero_sign_flag(assembler_, out_reg, !n.zero().targets().empty(), !n.negative().targets().empty());

		return out_reg;
	}

	auto amount = *materialise(n.amount().owner());

	switch (n.op()) {
	case shift_op::lsl:
		sll(assembler_, out_reg, src_reg, amount);
		break;
	case shift_op::lsr:
		srl(assembler_, out_reg, src_reg, amount);
		break;
	case shift_op::asr:
		sra(assembler_, out_reg, src_reg, amount);
		break;
	}
	zero_sign_flag(assembler_, out_reg, !n.zero().targets().empty(), !n.negative().targets().empty());

	return out_reg;
}

TypedRegister &riscv64_translation_context::materialise_binary_arith(const binary_arith_node &n)
{
	bool works = (is_gpr_or_flag(n.val()) && is_gpr_or_flag(n.lhs()) && is_gpr_or_flag(n.rhs()))
		|| ((n.op() == binary_arith_op::mul || n.op() == binary_arith_op::div || n.op() == binary_arith_op::mod || n.op() == binary_arith_op::bxor)
			&& is_i128(n.val()) && is_i128(n.lhs()) && is_i128(n.rhs()))
		|| ((is_int_vector(n.val(), 4, 32)) && (n.op() == binary_arith_op::add || n.op() == binary_arith_op::sub));
	if (!works) {
		throw std::runtime_error("unsupported width on binary arith operation");
	}
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}

	TypedRegister &src_reg1 = *materialise(n.lhs().owner());

	bool z_needed = !n.zero().targets().empty();
	bool v_needed = !n.overflow().targets().empty();
	bool c_needed = !n.carry().targets().empty();
	bool n_needed = !n.negative().targets().empty();
	bool flags_needed = z_needed || v_needed || c_needed || n_needed;

	const std::optional<int64_t> &i = get_as_int(n.rhs().owner());

	if (i) {
		// Could also work for LHS except sub
		auto imm = *i;
		// imm==0 more efficient as x0. Only IType works
		if (imm && IsITypeImm(imm)) {
			switch (n.op()) {
			case binary_arith_op::sub:
				if (imm == -2048) { // Can happen with inversion
					goto standardPath;
				}
				addi(assembler_, out_reg, src_reg1, -imm);
				subi_flags(assembler_, out_reg, src_reg1, imm, z_needed, v_needed, c_needed, n_needed);
				break;
			case binary_arith_op::add:
				addi(assembler_, out_reg, src_reg1, imm);
				addi_flags(assembler_, out_reg, src_reg1, imm, z_needed, v_needed, c_needed, n_needed);
				break;

			// Binary operations preserve sign extension, so we keep the effective input type (even if larger)
			case binary_arith_op::band:
				assembler_.andi(out_reg, src_reg1, imm);
				out_reg.set_actual_width();
				out_reg.set_type(src_reg1.type());
				zero_sign_flag(assembler_, out_reg, z_needed, n_needed);
				break;
			case binary_arith_op::bor:
				assembler_.ori(out_reg, src_reg1, imm);
				out_reg.set_actual_width();
				out_reg.set_type(src_reg1.type());
				zero_sign_flag(assembler_, out_reg, z_needed, n_needed);
				break;
			case binary_arith_op::bxor:
				assembler_.xori(out_reg, src_reg1, imm);
				out_reg.set_actual_width();
				out_reg.set_type(src_reg1.type());
				zero_sign_flag(assembler_, out_reg, z_needed, n_needed);
				break;
			default:
				// No-op Go to standard path
				goto standardPath;
			}
			return out_reg;
		}
	}

standardPath:
	TypedRegister &src_reg2 = *materialise(n.rhs().owner());
	switch (n.op()) {

	case binary_arith_op::add:
		add(assembler_, out_reg, src_reg1, src_reg2);
		add_flags(assembler_, out_reg, src_reg1, src_reg2, z_needed, v_needed, c_needed, n_needed);
		break;
	case binary_arith_op::sub:
		sub(assembler_, out_reg, src_reg1, src_reg2);
		sub_flags(assembler_, out_reg, src_reg1, src_reg2, z_needed, v_needed, c_needed, n_needed);
		break;
	// Binary operations preserve sign extension, so we can keep smaller of effective input types
	case binary_arith_op::band:
		assembler_.and_(out_reg, src_reg1, src_reg2);
		out_reg.set_actual_width();
		out_reg.set_type(get_minimal_type(src_reg1, src_reg2));
		zero_sign_flag(assembler_, out_reg, z_needed, n_needed);
		break;
	case binary_arith_op::bor:
		assembler_.or_(out_reg, src_reg1, src_reg2);
		out_reg.set_actual_width();
		out_reg.set_type(get_minimal_type(src_reg1, src_reg2));
		zero_sign_flag(assembler_, out_reg, z_needed, n_needed);
		break;
	case binary_arith_op::bxor:
		xor_(assembler_, out_reg, src_reg1, src_reg2);
		zero_sign_flag(assembler_, out_reg, z_needed, n_needed);
		break;
	case binary_arith_op::mul:
		// FIXME
		if (n.val().type().element_width() == 128 && (n.lhs().owner()->kind() != node_kinds::cast || n.rhs().owner()->kind() != node_kinds::cast)) {
			throw std::runtime_error("128bit multiply without cast");
		}
		mul(assembler_, out_reg, src_reg1, src_reg2);
		mul_flags(assembler_, out_reg, v_needed, c_needed);
		break;
	case binary_arith_op::div:
		div(assembler_, out_reg, src_reg1, src_reg2);
		break;
	case binary_arith_op::mod:
		mod(assembler_, out_reg, src_reg1, src_reg2);
		break;
	case binary_arith_op::cmpeq: {
		Register tmp = static_cast<const Register>(src_reg2) != ZERO ? out_reg : src_reg1;
		if (static_cast<const Register>(src_reg2) != ZERO) {
			assembler_.xor_(out_reg, src_reg1, src_reg2);
		}
		assembler_.seqz(out_reg, tmp);
		out_reg.set_actual_width(src_reg1.actual_width() < src_reg2.actual_width() ? src_reg1.actual_width_0() : src_reg2.actual_width_0());
		out_reg.set_type(get_minimal_type(src_reg1, src_reg2));
		return out_reg;
	}
	case binary_arith_op::cmpne: {
		Register tmp = static_cast<const Register>(src_reg2) != ZERO ? out_reg : src_reg1;
		if (static_cast<const Register>(src_reg2) != ZERO) {
			assembler_.xor_(out_reg, src_reg1, src_reg2);
		}
		assembler_.snez(out_reg, tmp);
		out_reg.set_actual_width(src_reg1.actual_width() < src_reg2.actual_width() ? src_reg1.actual_width_0() : src_reg2.actual_width_0());
		out_reg.set_type(get_minimal_type(src_reg1, src_reg2));
		return out_reg;
	}
	case binary_arith_op::cmpgt:
		throw std::runtime_error("unsupported binary arithmetic operation");
	}

	return out_reg;
}

TypedRegister &riscv64_translation_context::materialise_constant(int64_t imm)
{
	// Optimizations with left or right shift at the end not implemented (for constants with trailing or leading zeroes)

	if (imm == 0) {
		return allocate_register(nullptr, ZERO).first;
	}

	TypedRegister &reg = allocate_register(nullptr).first;
	gen_constant(assembler_, imm, reg);
	return reg;
}

TypedRegister &riscv64_translation_context::materialise_csel(const csel_node &n)
{
	if (!(is_gpr(n.val()) && is_gpr_or_flag(n.condition()) && is_gpr(n.falseval()) && is_gpr(n.trueval()))) {
		throw std::runtime_error("unsupported width on csel operation");
	}
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}

	Label false_calc {}, end {};

	TypedRegister &cond = *materialise(n.condition().owner());

	extend_to_64(assembler_, cond, cond);
	assembler_.beqz(cond, &false_calc);

	TypedRegister &trueval = *materialise(n.trueval().owner());
	assembler_.mv(out_reg, trueval);

	assembler_.j(&end);

	assembler_.Bind(&false_calc);

	TypedRegister &falseval = *materialise(n.falseval().owner());
	assembler_.mv(out_reg, falseval);

	assembler_.Bind(&end);

	// In-types might be wider than out-type so out accurate to narrower of the two
	out_reg.set_type(get_minimal_type(trueval, falseval));
	out_reg.set_actual_width(trueval.actual_width() < falseval.actual_width() ? trueval.actual_width_0() : falseval.actual_width_0());
	return out_reg;
}

void riscv64_translation_context::materialise_internal_call(const internal_call_node &n)
{
	const auto &function = n.fn();
	if (function.name() == "handle_syscall") {

		assembler_.sd(materialise_constant(current_address_ + 2), { FP, static_cast<intptr_t>(reg_offsets::PC) });
		ret_val_ = 1;
	} else if (function.name() == "handle_int") {
		// TODO handle argument
		ret_val_ = 2;
	} else {
		throw std::runtime_error("unsupported internal call");
	}
}

// FIXME 512
TypedRegister &riscv64_translation_context::materialise_vector_insert(const vector_insert_node &n)
{
	if (!(n.val().type().width() == 128 || n.val().type().width() == 512)) {
		throw std::runtime_error("Unsupported vector insert width");
	}
	if (!(n.val().type().element_width() == 64 || n.val().type().element_width() == 128 || n.val().type().element_width() == 32)) {
		throw std::runtime_error("Unsupported vector insert element width");
	}
	if (!(n.insert_value().type().width() == 64 || n.insert_value().type().width() == 128 || n.insert_value().type().width() == 32)) {
		throw std::runtime_error("Unsupported vector insert insert_value width");
	}

	TypedRegister &insert = *materialise(n.insert_value().owner());
	TypedRegister &src = *materialise(n.source_vector().owner());

	switch (n.source_vector().type().element_width()) {
	case 128: {
		if (n.index() == 0) {
			return insert;
		} else {
			throw std::runtime_error("Unsupported index for 512bit insert");
		}
	}
	case 64:
		if (n.index() == 0) {
			// No value modification just map correctly
			return allocate_register(&n.val(), insert.reg1(), src.reg2()).first;
		} else if (n.index() == 1) {
			// No value modification just map correctly
			return allocate_register(&n.val(), src.reg1(), insert.reg2()).first;
		} else {
			throw std::runtime_error("Unsupported vector insert index");
		}
		break;
	case 32: {
		switch (n.index()) {
		case 0:
		case 1: {
			auto [out_reg, valid] = allocate_register(&n.val(), std::nullopt, src.reg2());
			if (valid) {
				bit_insert(assembler_, out_reg, src, insert, 32 * n.index(), 32, allocate_register(nullptr).first);
			}
			return out_reg;
		}
		case 2:
		case 3: {
			auto [out_reg, valid] = allocate_register(&n.val(), src.reg1(), std::nullopt);
			if (valid) {
				bit_insert(assembler_, out_reg, src, insert, 32 * n.index(), 32, allocate_register(nullptr).first);
			}
			return out_reg;
		}
		default:
			throw std::runtime_error("Unsupported vector extract index");
		}
	}
	default:
		throw std::runtime_error("Unsupported vector insert element width");
	}
}

TypedRegister &riscv64_translation_context::materialise_vector_extract(const vector_extract_node &n)
{
	if (n.source_vector().type().width() != 128) {
		throw std::runtime_error("Unsupported vector extract width");
	}
	if (!(n.source_vector().type().element_width() == 64 || n.source_vector().type().element_width() == 32)) {
		throw std::runtime_error("Unsupported vector extract element width");
	}
	if (!(n.val().type().width() == 64 || n.val().type().width() == 32)) {
		throw std::runtime_error("Unsupported vector extract value width");
	}

	TypedRegister &src = *materialise(n.source_vector().owner());

	// When adding other widths, remember to set correct type

	switch (n.source_vector().type().element_width()) {
	case 64:
		if (n.index() == 0) {
			// No value modification just map correctly
			return allocate_register(&n.val(), src.reg1()).first;
		} else if (n.index() == 1) {
			// No value modification just map correctly
			return allocate_register(&n.val(), src.reg2()).first;
		} else {
			throw std::runtime_error("Unsupported vector extract index");
		}
		break;
	case 32: {

		auto [out_reg, valid] = allocate_register(&n.val());
		if (!valid) {
			return out_reg;
		}

		bit_extract(assembler_, out_reg, src, 32 * n.index(), 32);
		return out_reg;
	}
	default:
		throw std::runtime_error("Unsupported vector extract element width");
	}
}

/*
	 Instructions:
	 If FOLLOWING instruction is NOT a branch (so we do unconditional control flow or we are in the trueval part of conditional):
			If PREVIOUS instruction is NOT a nop:
			overwrite AUIPC with J (or if it doesn't fit with AUIPC + JR)
			If it is a NOP (previous instruction was an already chained branch, see below):
			overwrite previous instruction with J (or if it doesn't fit with AUIPC + JR)
	 If following instruction IS a branch (so it is falseval part of conditional control flow):
	 1a. overwrite AUIPC with that branch + adjusted offset to following block
	 1b. and the branch with a NOP
			if the instruction following the branch is a J OR an AUIPC + JR:
				move the J (or AUIPC + JR) in place of the NOP (trueval part already chained)
	 2. if offset doesn't fit:
			a) overwrite AUIPC with inverted branch + single ins offset
			b) the branch with unconditional branch to block
	 If that still doesn't fit:
			a) overwrite AUIPC with inverted branch + single ins offset
			b) move the constant generation of trueval by 1 instruction (falseval not needed anymore, can be overwritten)
			c) overwrite the branch with AUIPC
			d) overwrite the "hole" with JR
*/
void riscv64_translation_context::chain(uint64_t chain_address, void *chain_target)
{
	auto instr_p = reinterpret_cast<uint32_t *>(chain_address);
	uint32_t following_instr_enc = *(instr_p + 1);
	if (IsCInstruction(following_instr_enc)) {
		CInstruction following_instr { static_cast<uint16_t>(following_instr_enc) };
		if (following_instr.opcode() == C_BEQZ || following_instr.opcode() == C_BNEZ) {
			chain_machine_code_writer writer = chain_machine_code_writer { instr_p };

			Assembler ass { &writer, false, RV_GC };

			intptr_t offset = ass.offset_from_target(reinterpret_cast<intptr_t>(chain_target));

			if (!IsBTypeImm(offset)) {
				throw std::runtime_error("Chaining failed. Jump offset too big for single direct branch instruction.");
			} else {
				if (following_instr.opcode() == C_BEQZ) {
					ass.beqz(following_instr.rs1p(), offset);
				} else {
					ass.bnez(following_instr.rs1p(), offset);
				}

				ass.nop(IsCBImm(offset)); // AUIPC full and c_bxx half size, so replacement one full and one half instruction

				// TODO Check if next instruction is J or AUIPC + JR, move those inplace of the NOP
			}
		} else {
			// NOP before? Use the NOP
			chain_machine_code_writer writer = *(instr_p - 1) == 0b10011 ? chain_machine_code_writer { instr_p - 1 } : chain_machine_code_writer { instr_p };

			Assembler ass { &writer, false, RV_GC };
			intptr_t offset = ass.offset_from_target(reinterpret_cast<intptr_t>(chain_target));

			if (!IsJTypeImm(offset)) {
				throw std::runtime_error("Chaining failed. Jump offset too big for single direct jump instruction.");
			}
			ass.j(offset);
		}
	} else {
		Instruction following_instr { following_instr_enc };
		if (following_instr.opcode() == BRANCH) {
			chain_machine_code_writer writer = chain_machine_code_writer { instr_p };

			Assembler ass { &writer, false, RV_GC };

			intptr_t offset = ass.offset_from_target(reinterpret_cast<intptr_t>(chain_target));

			if (!IsBTypeImm(offset)) {
				throw std::runtime_error("Chaining failed. Jump offset too big for single direct branch instruction.");
			} else {
				switch (following_instr.funct3()) {
				case BEQ:
					ass.beq(following_instr.rs1(), following_instr.rs2(), offset);
					break;
				case BNE:
					ass.bne(following_instr.rs1(), following_instr.rs2(), offset);
					break;
				case BLT:
					ass.blt(following_instr.rs1(), following_instr.rs2(), offset);
					break;
				case BGE:
					ass.bge(following_instr.rs1(), following_instr.rs2(), offset);
					break;
				case BLTU:
					ass.bltu(following_instr.rs1(), following_instr.rs2(), offset);
					break;
				case BGEU:
					ass.bgeu(following_instr.rs1(), following_instr.rs2(), offset);
					break;

				default:
					throw std::runtime_error("Should not happen");
				}

				ass.nop(true); // AUIPC and BXX both full size, so both replacements full size

				// TODO Check if next instruction is J or AUIPC + JR, move those inplace of the NOP
			}
		} else {
			// NOP before? Use the NOP
			chain_machine_code_writer writer = *(instr_p - 1) == 0b10011 ? chain_machine_code_writer { instr_p - 1 } : chain_machine_code_writer { instr_p };

			Assembler ass { &writer, false, RV_GC };
			intptr_t offset = ass.offset_from_target(reinterpret_cast<intptr_t>(chain_target));

			if (!IsJTypeImm(offset)) {
				throw std::runtime_error("Chaining failed. Jump offset too big for single direct jump instruction.");
			}
			ass.j(offset);
		}
	}
}
