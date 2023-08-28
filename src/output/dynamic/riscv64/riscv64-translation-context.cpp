#include <arancini/ir/node.h>
#include <arancini/output/dynamic/riscv64/encoder/riscv64-constants.h>
#include <arancini/output/dynamic/riscv64/bitwise.h>
#include <arancini/output/dynamic/riscv64/riscv64-translation-context.h>
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
	constexpr static Register registers[] { S1, A0, A1, A2, A3, A4, A5, T0, T1, T2, A6, A7, S2, S3, S4, S5, S6, S7, T3, T4, T5 };

	if (reg_allocator_index_ >= std::size(registers)) {
		throw std::runtime_error("RISC-V DBT ran out of registers for packet");
	}

	if (!p) {
		Register r1 = reg1 ? *reg1 : registers[reg_allocator_index_++];
		temporaries.emplace_front(r1);
		return { temporaries.front(), true };
	}

	switch (p->type().width()) {
	case 512: // FIXME proper
	case 128: {
		Register r1 = reg1 ? *reg1 : registers[reg_allocator_index_++];
		Register r2 = reg2 ? *reg2 : registers[reg_allocator_index_++];
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
		Register r1 = reg1 ? *reg1 : registers[reg_allocator_index_++];
		auto [a, b] = reg_for_port_.emplace(p, r1);
		TypedRegister &tr = a->second;
		tr.set_type(p->type());
		return { tr, true };
	}
	default:
		throw std::runtime_error("Invalid type for register allocation");
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

std::optional<std::reference_wrapper<TypedRegister>> riscv64_translation_context::materialise(const node *n)
{
	if (!n) {
		throw std::runtime_error("RISC-V DBT received NULL pointer to node");
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
	case node_kinds::constant: {
		const constant_node &node = *reinterpret_cast<const constant_node *>(n);
		if (!is_gpr_or_flag(node.val())) {
			throw std::runtime_error("unsupported width on constant");
		}
		return materialise_constant((int64_t)node.const_val_i());
	}
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

Register riscv64_translation_context::materialise_ternary_atomic(const ternary_atomic_node &n)
{

	Register dstAddr = std::get<Register>(materialise(n.lhs().owner()));
	Register acc = std::get<Register>(materialise(n.rhs().owner()));
	Register src = std::get<Register>(materialise(n.top().owner()));
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}
	Label fail;
	Label retry;
	Label end;

	auto [reg, _] = allocate_register();

	assembler_.add(reg, dstAddr, MEM_BASE);

	auto addr = Address { reg };
	// FIXME Correct memory ordering?
	switch (n.op()) {

	case ternary_atomic_op::cmpxchg:
		switch (n.rhs().type().element_width()) {
		case 64:
			assembler_.Bind(&retry);

			assembler_.lrd(out_reg, addr, std::memory_order_acq_rel);
			assembler_.bne(out_reg, acc, &fail, Assembler::kNearJump);
			assembler_.scd(out_reg, src, addr, std::memory_order_acq_rel);
			assembler_.bnez(out_reg, &retry, Assembler::kNearJump);

			// Flags from comparison matching (i.e subtraction of equal values)
			assembler_.li(ZF, 1);
			assembler_.li(CF, 0);
			assembler_.li(OF, 0);
			assembler_.li(SF, 0);

			assembler_.j(&end, Assembler::kNearJump);

			assembler_.Bind(&fail);

			assembler_.sub(SF, acc, out_reg);

			assembler_.sgtz(CF, out_reg);
			assembler_.slt(OF, SF, acc);
			assembler_.xor_(OF, OF, CF); // OF

			assembler_.sltu(CF, acc, SF); // CF

			assembler_.li(ZF, 0); // ZF

			assembler_.sltz(SF, SF); // SF

			// Write back updated acc value
			assembler_.sd(out_reg, { FP, static_cast<intptr_t>(reinterpret_cast<read_reg_node *>(n.rhs().owner())->regoff()) });

			assembler_.Bind(&end);
			break;
		case 32:
			assembler_.Bind(&retry);

			assembler_.lrw(out_reg, addr, std::memory_order_acq_rel);
			assembler_.bne(out_reg, acc, &fail, Assembler::kNearJump);
			assembler_.scw(out_reg, src, addr, std::memory_order_acq_rel);
			assembler_.bnez(out_reg, &retry, Assembler::kNearJump);

			// Flags from comparison matching (i.e subtraction of equal values)
			assembler_.li(CF, 0);
			assembler_.li(OF, 0);
			assembler_.li(SF, 0);
			assembler_.li(ZF, 1);

			assembler_.j(&end, Assembler::kNearJump);

			assembler_.Bind(&fail);

			assembler_.subw(SF, acc, out_reg);

			assembler_.sgtz(CF, out_reg);
			assembler_.slt(OF, SF, acc);
			assembler_.xor_(OF, OF, CF); // OF

			assembler_.sltu(CF, acc, SF); // CF

			assembler_.li(ZF, 0); // ZF

			assembler_.sltz(SF, SF); // SF

			// Write back updated acc value
			assembler_.sw(out_reg, { FP, static_cast<intptr_t>(reinterpret_cast<read_reg_node *>(n.rhs().owner())->regoff()) });

			assembler_.Bind(&end);
			break;
		default:
			throw std::runtime_error("unsupported cmpxchg width");
		}
		return out_reg;
	default:
		throw std::runtime_error("unsupported ternary atomic operation");
	}
}

std::variant<Register, std::monostate> riscv64_translation_context::materialise_binary_atomic(const binary_atomic_node &n)
{
	Register dstAddr = std::get<Register>(materialise(n.lhs().owner()));
	Register src = std::get<Register>(materialise(n.rhs().owner()));
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}

	auto [reg, _] = allocate_register();

	assembler_.add(reg, dstAddr, MEM_BASE);

	auto addr = Address { reg };
	// FIXME Correct memory ordering?
	switch (n.op()) {
	case binary_atomic_op::xadd:
		switch (n.rhs().type().element_width()) {
		case 64:
			assembler_.amoaddd(out_reg, src, addr, std::memory_order_acq_rel);
			assembler_.add(SF, out_reg, src); // Actual sum for flag generation
			break;
		case 32:
			assembler_.amoaddw(out_reg, src, addr, std::memory_order_acq_rel);
			assembler_.addw(SF, out_reg, src); // Actual sum for flag generation
			break;
		default:
			throw std::runtime_error("unsupported xadd width");
		}

		assembler_.sltz(CF, src);
		assembler_.slt(OF, SF, out_reg);
		assembler_.xor_(OF, OF, CF); // OF

		assembler_.sltu(CF, SF, out_reg); // CF

		assembler_.seqz(ZF, SF); // ZF
		assembler_.sltz(SF, SF); // SF

		switch (n.rhs().type().element_width()) {
		case 64:
			assembler_.sd(out_reg, { FP, static_cast<intptr_t>(reinterpret_cast<read_reg_node *>(n.rhs().owner())->regoff()) });
			break;
		case 32:
			assembler_.slli(out_reg, out_reg, 32);
			assembler_.srli(out_reg, out_reg, 32);
			assembler_.sd(out_reg, { FP, static_cast<intptr_t>(reinterpret_cast<read_reg_node *>(n.rhs().owner())->regoff()) });
			break;
		default:
			throw std::runtime_error("unsupported xadd width");
		}
		return out_reg;
	case binary_atomic_op::add:
		switch (n.rhs().type().element_width()) {
		case 64:
			assembler_.amoaddd(out_reg, src, addr, std::memory_order_acq_rel);

			assembler_.add(SF, out_reg, src); // Actual sum for flag generation
			break;
		case 32:
			assembler_.amoaddw(out_reg, src, addr, std::memory_order_acq_rel);

			assembler_.addw(SF, out_reg, src); // Actual sum for flag generation
			break;
		default:
			throw std::runtime_error("unsupported lock add width");
		}

		assembler_.sltz(CF, src);
		assembler_.slt(OF, SF, out_reg);
		assembler_.xor_(OF, OF, CF); // OF

		assembler_.sltu(CF, SF, out_reg); // CF

		assembler_.seqz(ZF, SF); // ZF
		assembler_.sltz(SF, SF); // SF
		return std::monostate {};
	case binary_atomic_op::sub:
		assembler_.neg(out_reg, src);
		switch (n.rhs().type().element_width()) {
		case 64:
			assembler_.amoaddd(out_reg, out_reg, addr, std::memory_order_acq_rel);

			assembler_.sub(SF, out_reg, src); // Actual difference for flag generation
			break;
		case 32:
			assembler_.amoaddw(out_reg, src, addr, std::memory_order_acq_rel);

			assembler_.subw(SF, out_reg, src); // Actual difference for flag generation
			break;
		default:
			throw std::runtime_error("unsupported lock sub width");
		}

		assembler_.sgtz(CF, src);
		assembler_.slt(OF, SF, out_reg);
		assembler_.xor_(OF, OF, CF); // OF

		assembler_.sltu(CF, out_reg, SF); // CF

		assembler_.seqz(ZF, SF); // ZF
		assembler_.sltz(SF, SF); // SF
		return std::monostate {};
	case binary_atomic_op::band:
		switch (n.rhs().type().element_width()) {
		case 64:
			assembler_.amoandd(out_reg, src, addr, std::memory_order_acq_rel);

			assembler_.and_(SF, out_reg, src); // Actual and for flag generation
			break;
		case 32:
			assembler_.amoandw(out_reg, src, addr, std::memory_order_acq_rel);

			assembler_.and_(SF, out_reg, src); // Actual and for flag generation
			assembler_.slli(SF, SF, 32); // Get rid of higher 32 bits
			break;
		default:
			throw std::runtime_error("unsupported lock and width");
		}

		assembler_.seqz(ZF, SF); // ZF
		assembler_.sltz(SF, SF); // SF
		return std::monostate {};
	case binary_atomic_op::bor:
		switch (n.rhs().type().element_width()) {
		case 64:
			assembler_.amoord(out_reg, src, addr, std::memory_order_acq_rel);

			assembler_.or_(SF, out_reg, src); // Actual or for flag generation
			break;
		case 32:
			assembler_.amoorw(out_reg, src, addr, std::memory_order_acq_rel);

			assembler_.or_(SF, out_reg, src); // Actual or for flag generation
			assembler_.slli(SF, SF, 32); // Get rid of higher 32 bits
			break;
		default:
			throw std::runtime_error("unsupported lock or width");
		}

		assembler_.seqz(ZF, SF); // ZF
		assembler_.sltz(SF, SF); // SF
		return std::monostate {};
	case binary_atomic_op::bxor:
		switch (n.rhs().type().element_width()) {
		case 64:
			assembler_.amoxord(out_reg, src, addr, std::memory_order_acq_rel);

			assembler_.xor_(SF, out_reg, src); // Actual xor for flag generation
			break;
		case 32:
			assembler_.amoxorw(out_reg, src, addr, std::memory_order_acq_rel);

			assembler_.xor_(SF, out_reg, src); // Actual xor for flag generation
			assembler_.slli(SF, SF, 32); // Get rid of higher 32 bits
			break;
		default:
			throw std::runtime_error("unsupported lock xor width");
		}

		assembler_.seqz(ZF, SF); // ZF
		assembler_.sltz(SF, SF); // SF
		return std::monostate {};
	case binary_atomic_op::xchg:
		switch (n.rhs().type().element_width()) {
		case 64:
			assembler_.amoswapd(out_reg, src, addr, std::memory_order_acq_rel);
			break;
		case 32:
			assembler_.amoswapw(out_reg, src, addr, std::memory_order_acq_rel);
			break;
		default:
			throw std::runtime_error("unsupported xchg width");
		}

		switch (n.rhs().type().element_width()) {
		case 64:
			assembler_.sd(out_reg, { FP, static_cast<intptr_t>(reinterpret_cast<read_reg_node *>(n.rhs().owner())->regoff()) });
			break;
		case 32:
			assembler_.slli(out_reg, out_reg, 32);
			assembler_.srli(out_reg, out_reg, 32);
			assembler_.sd(out_reg, { FP, static_cast<intptr_t>(reinterpret_cast<read_reg_node *>(n.rhs().owner())->regoff()) });
			break;
		default:
			throw std::runtime_error("unsupported xchg width");
		}
		return out_reg;
	default:
		throw std::runtime_error("unsupported binary atomic operation");
	}
}

Register riscv64_translation_context::materialise_cast(const cast_node &n)
{
	Register src_reg = std::get<Register>(materialise(n.source_value().owner()));

	bool works = (is_scalar_int(n.val()) || ((is_int_vector(n.val(), 128, 64) || is_int_vector(n.val(), 128, 32)) && n.op() == cast_op::bitcast))
		&& (is_gpr_or_flag(n.source_value()) || (is_i128(n.source_value()) && (n.op() == cast_op::trunc || n.op() == cast_op::bitcast)));
	if (!works) {
		throw std::runtime_error("unsupported types on cast operation");
	}

	switch (n.op()) {

	case cast_op::bitcast:
		if (n.val().type().width() == 128) { //Needs to be width so casting to vector works
			secondary_reg_for_port_[&n.val()] = get_secondary_register(&n.source_value()).encoding();
		}
		return src_reg;
	case cast_op::sx:
		if (is_i128(n.val())) {
			for (const node *target : n.val().targets()) {
				if (target->kind() != node_kinds::binary_arith) {
					throw std::runtime_error("unsupported types on cast sx operation");
				}
				binary_arith_op op = ((const binary_arith_node *)target)->op();
				if (!(op == binary_arith_op::mul || op == binary_arith_op::div || op == binary_arith_op::mod)) {
					throw std::runtime_error("unsupported types on cast sx operation");
				}
			}
		}
		// No-op
		return src_reg;

	case cast_op::zx: {
		if (n.val().type().element_width() == 128) {
			secondary_reg_for_port_[&n.val()] = ZERO.encoding();
			if (n.source_value().type().element_width() == 64) {
				return src_reg;
			}
		}
		if (is_flag(n.source_value())) { // Flags always zero extended
			return src_reg;
		}
		auto [out_reg, valid] = allocate_register(&n.val());
		if (!valid) {
			return out_reg;
		}
		if (n.source_value().type().element_width() == 8) {
			assembler_.andi(out_reg, src_reg, 0xff);
		} else {
			assembler_.slli(out_reg, src_reg, 64 - n.source_value().type().element_width());
			assembler_.srli(out_reg, out_reg, 64 - n.source_value().type().element_width()));
		}
		return out_reg;
	}
	case cast_op::trunc: {
		if (is_i128(n.val())) {
			throw std::runtime_error("truncate to 128bit unsupported");
		}

		if (n.val().type().element_width() == 64) {
			return src_reg;
		}

		auto [out_reg, valid] = allocate_register(&n.val());
		if (!valid) {
			return out_reg;
		}

		if (is_flag(n.val())) {
			// Flags are always zero extended
			assembler_.andi(out_reg, src_reg, 1);
		} // Sign extend the truncated bits to preserve convention
		else if (n.val().type().element_width() == 32) {
			assembler_.sextw(out_reg, src_reg);
		} else {
			assembler_.slli(out_reg, src_reg, 64 - n.val().type().element_width());
			assembler_.srai(out_reg, out_reg, 64 - n.val().type().element_width());
		}
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
		} else if (to + length < 64) {
			// Map upper and insert into lower
			auto [out_reg, valid] = allocate_register(&n.val(), std::nullopt, src.reg2());
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
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}
	if (is_flag(value) || is_gpr(value)) { // Flags or GPR
		auto load_instr = load_instructions.at(value.type().element_width());
		(assembler_.*load_instr)(out_reg, { FP, static_cast<intptr_t>(n.regoff()) });
		if (!is_flag(value)) {
			out_reg.set_actual_width();
		}
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
	if (is_flag(value) || is_gpr(value)) { // Flags or GPR
		auto store_instr = store_instructions.at(value.type().element_width());
		if (is_flag(value)) {
			Register reg = (!is_flag_port(value)) ? (materialise(value.owner()))->get() : Register { flag_map.at(n.regoff()) };
			if (is_flag_port(value) && !reg_for_port_.count(&value.owner()->val())) {
				// Result of node not written only flags needed
				materialise(value.owner());
			}
			(assembler_.*store_instr)(reg, { FP, static_cast<intptr_t>(n.regoff()) });
		} else {
			TypedRegister &reg = *(materialise(value.owner()));

			(assembler_.*store_instr)(reg, { FP, static_cast<intptr_t>(n.regoff()) });
		}
		return;
	} else if (is_i128(value) || is_int_vector(value, 128, 64) || is_int_vector(value, 128, 32) || is_int(value, 512) || is_int_vector(value, 512, 128)) {
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
	// FIXME maybe only support 64bit values for PC
	TypedRegister &src_reg = *materialise(n.value().owner());

	Address addr { FP, static_cast<intptr_t>(reg_offsets::PC) };
	auto store_instr = store_instructions.at(n.value().type().element_width());
	(assembler_.*store_instr)(src_reg, addr);
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

Register riscv64_translation_context::materialise_unary_arith(const unary_arith_node &n)
{
	if (!(is_gpr_or_flag(n.val()) && is_gpr_or_flag(n.lhs()))) {
		throw std::runtime_error("unsupported width on unary arith operation");
	}
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}

	Register src_reg = std::get<Register>(materialise(n.lhs().owner()));
	if (n.op() == unary_arith_op::bnot) {
		if (is_flag(n.val())) {
			assembler_.xori(out_reg, src_reg, 1);
		} else {
			assembler_.not_(out_reg, src_reg);
		}
		return out_reg;
	}

	throw std::runtime_error("unsupported unary arithmetic operation");
}

Register riscv64_translation_context::materialise_ternary_arith(const ternary_arith_node &n)
{
	if (!(is_gpr(n.val()) && is_gpr(n.lhs()) && is_gpr(n.rhs()) && is_gpr(n.top()))) {
		throw std::runtime_error("unsupported width on ternary arith operation");
	}
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}

	Register src_reg1 = std::get<Register>(materialise(n.lhs().owner()));
	Register src_reg2 = std::get<Register>(materialise(n.rhs().owner()));
	Register src_reg3 = std::get<Register>(materialise(n.top().owner()));
	switch (n.op()) {
		// TODO Immediate handling

	case ternary_arith_op::adc:
		// Temporary: Add carry in
		switch (n.val().type().element_width()) {
		case 64:
			assembler_.add(ZF, src_reg2, src_reg3);
			break;
		case 32:
			assembler_.addw(ZF, src_reg2, src_reg3);
			break;
		case 8:
		case 16:
			assembler_.add(ZF, src_reg2, src_reg3);
			assembler_.slli(ZF, ZF, 64 - n.val().type().element_width());
			assembler_.srai(ZF, ZF, 64 - n.val().type().element_width());
			break;
		}

		assembler_.slt(OF, ZF, src_reg2); // Temporary overflow
		assembler_.sltu(CF, ZF, src_reg2); // Temporary carry

		// Normal add
		switch (n.val().type().element_width()) {
		case 64:
			assembler_.add(out_reg, src_reg1, ZF);
			break;
		case 32:
			assembler_.addw(out_reg, src_reg1, ZF);
			break;
		case 8:
		case 16:
			assembler_.add(out_reg, src_reg1, ZF);
			assembler_.slli(out_reg, out_reg, 64 - n.val().type().element_width());
			assembler_.srai(out_reg, out_reg, 64 - n.val().type().element_width());
			break;
		}

		assembler_.sltu(SF, out_reg, ZF); // Normal carry out
		assembler_.or_(CF, CF, SF); // Total carry out

		assembler_.sltz(SF, src_reg1);
		assembler_.slt(ZF, out_reg, ZF);
		assembler_.xor_(ZF, ZF, SF); // Normal overflow out
		assembler_.xor_(OF, OF, ZF); // Total overflow out

		break;
	case ternary_arith_op::sbb:
		// Temporary: Add carry in
		switch (n.val().type().element_width()) {
		case 64:
			assembler_.add(ZF, src_reg2, src_reg3);
			break;
		case 32:
			assembler_.addw(ZF, src_reg2, src_reg3);
			break;
		case 8:
		case 16:
			assembler_.add(ZF, src_reg2, src_reg3);
			assembler_.slli(ZF, ZF, 64 - n.val().type().element_width());
			assembler_.srai(ZF, ZF, 64 - n.val().type().element_width());
			break;
		}

		assembler_.slt(OF, ZF, src_reg2); // Temporary overflow
		assembler_.sltu(CF, ZF, src_reg2); // Temporary carry

		switch (n.val().type().element_width()) {
		case 64:
			assembler_.sub(out_reg, src_reg1, ZF);
			break;
		case 32:
			assembler_.subw(out_reg, src_reg1, ZF);
			break;
		case 16:
			assembler_.sub(out_reg, src_reg1, ZF);
			assembler_.slli(out_reg, out_reg, 64 - n.val().type().element_width());
			assembler_.srai(out_reg, out_reg, 64 - n.val().type().element_width());
			break;
		}

		assembler_.sltu(SF, src_reg1, out_reg); // Normal carry out
		assembler_.or_(CF, CF, SF); // Total carry out

		assembler_.sgtz(SF, ZF);
		assembler_.slt(ZF, out_reg, src_reg1);
		assembler_.xor_(ZF, ZF, SF); // Normal overflow out
		assembler_.xor_(OF, OF, ZF); // Total overflow out

		break;
	default:
		throw std::runtime_error("unsupported ternary arithmetic operation");
	}

	assembler_.seqz(ZF, out_reg); // ZF
	assembler_.sltz(SF, out_reg); // SF

	return out_reg;
}

Register riscv64_translation_context::materialise_bit_shift(const bit_shift_node &n)
{
	if (!(is_gpr(n.val()) && is_gpr(n.input()) && is_gpr(n.amount()))) {
		throw std::runtime_error("unsupported width on bit shift operation");
	}
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}

	Register src_reg = std::get<Register>(materialise(n.input().owner()));

	if (n.amount().kind() == port_kinds::constant) {
		auto amt = (intptr_t)((constant_node *)n.amount().owner())->const_val_i() & 0x3f;
		if (amt == 0) {
			return src_reg;
		}
		switch (n.op()) {
		case shift_op::lsl:
			/*
			assembler_.srli(CF, src_reg, n.val().type().width() - amt); // CF

			if (amt == 1) {
				assembler_.srli(OF, src_reg, n.val().type().width() - amt - 1); // OF
				assembler_.xor_(OF, OF, CF);
				assembler_.andi(OF, OF, 1);
			}
			*/
			switch (n.val().type().element_width()) {
			case 64:
				assembler_.slli(out_reg, src_reg, amt);
				break;
			case 32:
				assembler_.slliw(out_reg, src_reg, amt);
				break;
			case 8:
			case 16:
				assembler_.slli(out_reg, src_reg, amt + (64 - n.val().type().element_width()));
				assembler_.srai(out_reg, out_reg, (64 - n.val().type().element_width()));
				break;
			}

			break;
		case shift_op::lsr:
			//			assembler_.srli(CF, src_reg, amt - 1); // CF
			switch (n.val().type().element_width()) {
			case 64:
				assembler_.srli(out_reg, src_reg, amt);
				break;
			case 32:
				assembler_.srliw(out_reg, src_reg, amt);
				break;
			case 16:
				assembler_.slli(out_reg, src_reg, 48);
				assembler_.srli(out_reg, out_reg, 48 + amt);
				break;
			case 8:
				assembler_.andi(out_reg, src_reg, 0xff);
				assembler_.srli(out_reg, out_reg, amt);
				break;
			}
			/*if (amt == 1) {
				assembler_.srli(OF, src_reg, n.val().type().width() - 1);
				assembler_.andi(OF, OF, 1); // OF
			}*/
			break;
		case shift_op::asr:
			//			assembler_.srli(CF, src_reg, amt - 1); // CF
			assembler_.srai(out_reg, src_reg, amt); // Sign extension preserved
			if (amt == 1) {
				assembler_.li(OF, 0);
			}
			break;
		}
		/*
		assembler_.andi(CF, CF, 1); // CF
		*/
		assembler_.seqz(ZF, out_reg); // ZF
		assembler_.sltz(SF, out_reg); // SF

		return out_reg;
	}

	auto amount = std::get<Register>(materialise(n.amount().owner()));
	//	assembler_.subi(OF, amount, 1);

	switch (n.op()) {
	case shift_op::lsl:
		//		assembler_.sll(CF, src_reg, OF); // CF in highest bit

		switch (n.val().type().element_width()) {
		case 64:
			assembler_.sll(out_reg, src_reg, amount);
			break;
		case 32:
			assembler_.sllw(out_reg, src_reg, amount);
			break;
		case 8:
		case 16:
			assembler_.sll(out_reg, src_reg, amount);
			assembler_.slli(out_reg, out_reg, (64 - n.val().type().element_width()));
			assembler_.srai(out_reg, out_reg, (64 - n.val().type().element_width()));
			break;
		}
		/*
		assembler_.xor_(OF, out_reg, CF); // OF in highest bit

		assembler_.srli(CF, CF, n.val().type().width() - 1);

		assembler_.srli(OF, OF, n.val().type().width() - 1);
		assembler_.andi(OF, OF, 1); // OF
		*/
		break;
	case shift_op::lsr:
		//		assembler_.srl(CF, src_reg, OF); // CF
		switch (n.val().type().element_width()) {
		case 64:
			assembler_.srl(out_reg, src_reg, amount);
			break;
		case 32:
			assembler_.srlw(out_reg, src_reg, amount);
			break;
		case 16:
			assembler_.slli(out_reg, src_reg, 48);
			assembler_.srli(out_reg, out_reg, 48);
			assembler_.srl(out_reg, out_reg, amount);
			break;
		case 8:
			assembler_.andi(out_reg, src_reg, 0xff);
			assembler_.srl(out_reg, src_reg, amount);
			break;
		}
		/*
		// Undefined on shift amt > 1 (use same formula as amt == 1)
		assembler_.srli(OF, src_reg, n.val().type().width() - 1);
		assembler_.andi(OF, OF, 1); // OF
		*/

		break;
	case shift_op::asr:
		/*
		assembler_.srl(CF, src_reg, OF); // CF
		assembler_.li(OF, 0); // OF
		*/

		assembler_.sra(out_reg, src_reg, amount); // Sign extension preserved

		break;
	}
	/*
	assembler_.andi(CF, CF, 1); // Limit CF to single bit
	*/
	assembler_.seqz(ZF, out_reg); // ZF
	assembler_.sltz(SF, out_reg); // SF

	return out_reg;
}

Register riscv64_translation_context::materialise_binary_arith(const binary_arith_node &n)
{
	bool works = (is_gpr_or_flag(n.val()) && is_gpr_or_flag(n.lhs()) && is_gpr_or_flag(n.rhs()))
		|| ((n.op() == binary_arith_op::mul || n.op() == binary_arith_op::div || n.op() == binary_arith_op::mod || n.op() == binary_arith_op::bxor)
			&& is_i128(n.val()) && is_i128(n.lhs()) && is_i128(n.rhs()))
		|| ((is_int_vector(n.val(), 128, 32)) && n.op() == binary_arith_op::add);
	if (!works) {
		throw std::runtime_error("unsupported width on binary arith operation");
	}
	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}

	Register src_reg1 = std::get<Register>(materialise(n.lhs().owner()));

	bool flags_needed = !(n.zero().targets().empty() && n.overflow().targets().empty() && n.carry().targets().empty() && n.negative().targets().empty());

	if (n.rhs().owner()->kind() == node_kinds::constant) {
		// Could also work for LHS except sub
		// TODO Probably incorrect to just cast to signed 64bit
		auto imm = (intptr_t)((constant_node *)(n.rhs().owner()))->const_val_i();
		// imm==0 more efficient as x0. Only IType works
		if (imm && IsITypeImm(imm)) {
			switch (n.op()) {

			case binary_arith_op::sub:
				if (imm == -2048) { // Can happen with inversion
					goto standardPath;
				}
				switch (n.val().type().element_width()) {
				case 64:
					assembler_.addi(out_reg, src_reg1, -imm);
					break;
				case 32:
					assembler_.addiw(out_reg, src_reg1, -imm);
					break;
				case 8:
				case 16:
					assembler_.addi(out_reg, src_reg1, -imm);
					assembler_.slli(out_reg, out_reg, 64 - n.val().type().element_width());
					assembler_.srai(out_reg, out_reg, 64 - n.val().type().element_width());
					break;
				default:
					throw std::runtime_error("Unsupported width for sub immediate");
				}
				if (flags_needed) {
					assembler_.sltu(CF, src_reg1, out_reg); // CF FIXME Assumes out_reg!=src_reg1
					assembler_.slt(OF, out_reg, src_reg1); // OF FIXME Assumes out_reg!=src_reg1
					if (imm > 0) {
						assembler_.xori(OF, OF, 1); // Invert on positive
					}
				}
				break;

			case binary_arith_op::add:

				switch (n.val().type().element_width()) {
				case 64:
					assembler_.addi(out_reg, src_reg1, imm);
					break;
				case 32:
					assembler_.addiw(out_reg, src_reg1, imm);
					break;
				case 8:
				case 16:
					assembler_.addi(out_reg, src_reg1, imm);
					assembler_.slli(out_reg, out_reg, 64 - n.val().type().element_width());
					assembler_.srai(out_reg, out_reg, 64 - n.val().type().element_width());
					break;
				default:
					throw std::runtime_error("Unsupported width for sub immediate");
				}

				if (flags_needed) {
					assembler_.sltu(CF, out_reg, src_reg1); // CF FIXME Assumes out_reg!=src_reg1
					assembler_.slt(OF, out_reg, src_reg1); // OF FIXME Assumes out_reg!=src_reg1
					if (imm < 0) {
						assembler_.xori(OF, OF, 1); // Invert on negative
					}
				}

				break;
			// Binary operations preserve sign extension
			case binary_arith_op::band:
				assembler_.andi(out_reg, src_reg1, imm);
				break;
			case binary_arith_op::bor:
				assembler_.ori(out_reg, src_reg1, imm);
				break;
			case binary_arith_op::bxor:
				assembler_.xori(out_reg, src_reg1, imm);
				break;
			default:
				// No-op Go to standard path
				goto standardPath;
			}

			if (flags_needed) {
				assembler_.seqz(ZF, out_reg); // ZF
				assembler_.sltz(SF, out_reg); // SF
			}

			return out_reg;
		}
	}

standardPath:
	Register src_reg2 = std::get<Register>(materialise(n.rhs().owner()));
	switch (n.op()) {

	case binary_arith_op::add:
		switch (n.val().type().width()) { //Needs to stay width so vec4x32 works
		case 128: {
			if (is_int_vector(n.val(), 128, 32)) {
				Register src_reg22 = get_secondary_register(&n.rhs());
				Register src_reg12 = get_secondary_register(&n.rhs());
				Register out_reg2 = get_secondary_register(&n.val());

				assembler_.srli(CF, src_reg1, 32);
				assembler_.srli(OF, src_reg2, 32);
				assembler_.add(OF, OF, CF);
				assembler_.slli(OF, OF, 32);

				assembler_.add(out_reg, src_reg1, src_reg2);
				assembler_.slli(out_reg, out_reg, 32);
				assembler_.srli(out_reg, out_reg, 32);

				assembler_.or_(out_reg, out_reg, OF);

				assembler_.srli(CF, src_reg12, 32);
				assembler_.srli(OF, src_reg22, 32);
				assembler_.add(OF, OF, CF);
				assembler_.slli(OF, OF, 32);

				assembler_.add(out_reg2, src_reg12, src_reg22);
				assembler_.slli(out_reg2, out_reg2, 32);
				assembler_.srli(out_reg2, out_reg2, 32);

				assembler_.or_(out_reg2, out_reg2, OF);
			}
			// Else Should not happen
		} break;
		case 64:
			assembler_.add(out_reg, src_reg1, src_reg2);
			break;
		case 32:
			assembler_.addw(out_reg, src_reg1, src_reg2);
			break;
		case 8:
		case 16:
			assembler_.add(out_reg, src_reg1, src_reg2);
			assembler_.slli(out_reg, out_reg, 64 - n.val().type().element_width());
			assembler_.srai(out_reg, out_reg, 64 - n.val().type().element_width());
			break;
		default:
			throw std::runtime_error("Unsupported width for sub immediate");
		}

		if (flags_needed) {
			assembler_.sltz(CF, src_reg1);
			assembler_.slt(OF, out_reg, src_reg2);
			assembler_.xor_(OF, OF, CF); // OF FIXME Assumes out_reg!=src_reg1 && out_reg!=src_reg2

			assembler_.sltu(CF, out_reg, src_reg2); // CF (Allows typical x86 case of regSrc1==out_reg) FIXME Assumes out_reg!=src_reg2
		}
		break;

	case binary_arith_op::sub:
		switch (n.val().type().element_width()) {
		case 64:
			assembler_.sub(out_reg, src_reg1, src_reg2);
			break;
		case 32:
			assembler_.subw(out_reg, src_reg1, src_reg2);
			break;
		case 8:
		case 16:
			assembler_.sub(out_reg, src_reg1, src_reg2);
			assembler_.slli(out_reg, out_reg, 64 - n.val().type().element_width());
			assembler_.srai(out_reg, out_reg, 64 - n.val().type().element_width());
			break;
		default:
			throw std::runtime_error("Unsupported width for sub immediate");
		}

		if (flags_needed) {
			assembler_.sgtz(CF, src_reg2);
			assembler_.slt(OF, out_reg, src_reg1);
			assembler_.xor_(OF, OF, CF); // OF FIXME Assumes out_reg!=src_reg1 && out_reg!=src_reg2

			assembler_.sltu(CF, src_reg1, out_reg); // CF FIXME Assumes out_reg!=src_reg1
		}
		break;

	// Binary operations preserve sign extension
	case binary_arith_op::band:
		assembler_.and_(out_reg, src_reg1, src_reg2);
		break;
	case binary_arith_op::bor:
		assembler_.or_(out_reg, src_reg1, src_reg2);
		break;
	case binary_arith_op::bxor:
		assembler_.xor_(out_reg, src_reg1, src_reg2);
		if (is_i128(n.val())) {
			Register out_reg2 = get_secondary_register(&n.val());
			Register src_reg12 = get_secondary_register(&n.lhs());
			Register src_reg22 = get_secondary_register(&n.rhs());
			assembler_.xor_(out_reg2, src_reg12, src_reg22);
		}
		break;

	case binary_arith_op::mul:
		switch (n.val().type().element_width()) {
		case 128: {

			// FIXME
			if (n.lhs().owner()->kind() != node_kinds::cast || n.rhs().owner()->kind() != node_kinds::cast) {
				throw std::runtime_error("128bit multiply without cast");
			}

			Register out_reg2 = get_secondary_register(&n.val());
			// Split calculation
			assembler_.mul(out_reg, src_reg1, src_reg2);
			switch (n.val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler_.mulh(out_reg2, src_reg1, src_reg2);
				if (flags_needed) {
					assembler_.srai(CF, out_reg, 63);
					assembler_.xor_(CF, CF, out_reg2);
					assembler_.snez(CF, CF);
				}
				break;
			case value_type_class::unsigned_integer:
				assembler_.mulhu(out_reg2, src_reg1, src_reg2);
				if (flags_needed) {
					assembler_.snez(CF, out_reg2);
				}
				break;
			default:
				throw std::runtime_error("Unsupported value type for multiply");
			}
			if (flags_needed) {
				assembler_.mv(OF, CF);
			}
		} break;

		case 64:
		case 32:
		case 16:
			assembler_.mul(out_reg, src_reg1, src_reg2); // Assumes proper signed/unsigned extension from 32/16/8 bits

			if (flags_needed) {
				switch (n.val().type().element_type().type_class()) {

				case value_type_class::signed_integer:
					if (n.val().type().element_width() == 64) {
						assembler_.sextw(CF, out_reg);
					} else {
						assembler_.slli(CF, out_reg, 64 - (n.val().type().element_width()) / 2);
						assembler_.srai(CF, out_reg, 64 - (n.val().type().element_width()) / 2);
					}
					assembler_.xor_(CF, CF, out_reg);
					assembler_.snez(CF, CF);
					break;
				case value_type_class::unsigned_integer:
					assembler_.srli(CF, out_reg, n.val().type().element_width() / 2);
					assembler_.snez(CF, CF);
					break;
				default:
					throw std::runtime_error("Unsupported value type for multiply");
				}
				assembler_.mv(OF, CF);
			}
			break;
		default:
			throw std::runtime_error("Unsupported width for sub immediate");
		}
		break;
	case binary_arith_op::div:
		switch (n.val().type().element_width()) {
		case 128: // Fixme 128 bits not natively supported on RISCV, assuming just extended 64 bit value
		case 64:
			switch (n.val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler_.div(out_reg, src_reg1, src_reg2);
				break;
			case value_type_class::unsigned_integer:
				assembler_.divu(out_reg, src_reg1, src_reg2);
				break;
			default:
				throw std::runtime_error("Unsupported value type for divide");
			}
			break;
		case 32:
			switch (n.val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler_.divw(out_reg, src_reg1, src_reg2);
				break;
			case value_type_class::unsigned_integer:
				assembler_.divuw(out_reg, src_reg1, src_reg2);
				break;
			default:
				throw std::runtime_error("Unsupported value type for divide");
			}
			break;
		case 16:
			switch (n.val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler_.divw(out_reg, src_reg1, src_reg2);
				assembler_.slli(out_reg, out_reg, 48);
				assembler_.srai(out_reg, out_reg, 48);
				break;
			case value_type_class::unsigned_integer:
				assembler_.divuw(out_reg, src_reg1, src_reg2);
				assembler_.slli(out_reg, out_reg, 48);
				assembler_.srli(out_reg, out_reg, 48);
				break;
			default:
				throw std::runtime_error("Unsupported value type for divide");
			}
			break;
		default:
			throw std::runtime_error("Unsupported width for sub immediate");
		}
		break;
	case binary_arith_op::mod:
		switch (n.val().type().element_width()) {
		case 128: // Fixme 128 bits not natively supported on RISCV, assuming just extended 64 bit value
		case 64:
			switch (n.val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler_.rem(out_reg, src_reg1, src_reg2);
				break;
			case value_type_class::unsigned_integer:
				assembler_.remu(out_reg, src_reg1, src_reg2);
				break;
			default:
				throw std::runtime_error("Unsupported value type for divide");
			}
			break;
		case 32:
			switch (n.val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler_.remw(out_reg, src_reg1, src_reg2);
				break;
			case value_type_class::unsigned_integer:
				assembler_.remuw(out_reg, src_reg1, src_reg2);
				break;
			default:
				throw std::runtime_error("Unsupported value type for divide");
			}
			break;
		case 16:
			switch (n.val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler_.remw(out_reg, src_reg1, src_reg2);
				assembler_.slli(out_reg, out_reg, 48);
				assembler_.srai(out_reg, out_reg, 48);
				break;
			case value_type_class::unsigned_integer:
				assembler_.remuw(out_reg, src_reg1, src_reg2);
				assembler_.slli(out_reg, out_reg, 48);
				assembler_.srli(out_reg, out_reg, 48);
				break;
			default:
				throw std::runtime_error("Unsupported value type for divide");
			}
			break;
		default:
			throw std::runtime_error("Unsupported width for sub immediate");
		}
		break;

	case binary_arith_op::cmpeq: {
		Register tmp = src_reg2 != ZERO ? out_reg : src_reg1;
		if (src_reg2 != ZERO) {
			assembler_.xor_(out_reg, src_reg1, src_reg2);
		}
		assembler_.seqz(out_reg, tmp);
		return out_reg;
	}
	case binary_arith_op::cmpne: {
		Register tmp = src_reg2 != ZERO ? out_reg : src_reg1;
		if (src_reg2 != ZERO) {
			assembler_.xor_(out_reg, src_reg1, src_reg2);
		}
		assembler_.snez(out_reg, tmp);
		return out_reg;
	}
	case binary_arith_op::cmpgt:
		throw std::runtime_error("unsupported binary arithmetic operation");
	}

	// TODO those should only be set on add, sub, xor, or, and
	if (flags_needed) {
		assembler_.seqz(ZF, out_reg); // ZF
		assembler_.sltz(SF, out_reg); // SF
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
	if (n.val().type().width() != 128 && n.val().type().width() != 512) {
		throw std::runtime_error("Unsupported vector insert width");
	}
	if (n.val().type().element_width() != 64 && n.val().type().element_width() != 128) {
		throw std::runtime_error("Unsupported vector insert element width");
	}
	if (n.insert_value().type().width() != 64 && n.insert_value().type().width() != 128) {
		throw std::runtime_error("Unsupported vector insert insert_value width");
	}

	TypedRegister &insert = *materialise(n.insert_value().owner());
	TypedRegister &src = *materialise(n.source_vector().owner());

	if (n.index() == 0) {
		// No value modification just map correctly
		return allocate_register(&n.val(), insert.reg1(), src.reg2()).first;
	} else if (n.index() == 1) {
		// No value modification just map correctly
		return allocate_register(&n.val(), src.reg1(), insert.reg2()).first;
	} else {
		throw std::runtime_error("Unsupported vector insert index");
	}
}

TypedRegister &riscv64_translation_context::materialise_vector_extract(const vector_extract_node &n)
{
	if (n.source_vector().type().width() != 128) {
		throw std::runtime_error("Unsupported vector extract width");
	}
	if (n.source_vector().type().element_width() != 64) {
		throw std::runtime_error("Unsupported vector extract element width");
	}
	if (n.val().type().width() != 64) {
		throw std::runtime_error("Unsupported vector extract value width");
	}

	TypedRegister &src = *materialise(n.source_vector().owner());

	// When adding other widths, remember to set correct type
	if (n.index() == 0) {
		// No value modification just map correctly
		return allocate_register(&n.val(), src.reg1()).first;
	} else if (n.index() == 1) {
		// No value modification just map correctly
		return allocate_register(&n.val(), src.reg2()).first;
	} else {
		throw std::runtime_error("Unsupported vector extract index");
	}
}
