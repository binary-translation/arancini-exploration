#include <arancini/ir/node.h>
#include <arancini/output/dynamic/chain.h>
#include <arancini/output/dynamic/riscv64/arithmetic.h>
#include <arancini/output/dynamic/riscv64/bitwise.h>
#include <arancini/output/dynamic/riscv64/encoder/riscv64-constants.h>
#include <arancini/output/dynamic/riscv64/flags.h>
#include <arancini/output/dynamic/riscv64/instruction-builder/builder.h>
#include <arancini/output/dynamic/riscv64/register_usage_identifier.h>
#include <arancini/output/dynamic/riscv64/riscv64-translation-context.h>
#include <arancini/output/dynamic/riscv64/shift.h>
#include <arancini/output/dynamic/riscv64/utils.h>

#include <algorithm>
#include <unordered_map>

using namespace arancini::output::dynamic::riscv64;
using namespace arancini::ir;

/**
 * Translations assumes FP holds pointer to CPU state.
 */

static constexpr const unsigned long flag_idx = static_cast<const unsigned long>(std::min({ reg_idx::ZF, reg_idx::CF, reg_idx::OF, reg_idx::SF }));
static constexpr const unsigned long flag_off
	= static_cast<const unsigned long>(std::min({ reg_offsets::ZF, reg_offsets::CF, reg_offsets::OF, reg_offsets::SF }));

template <const size_t order[], typename Tuple, size_t... idxs> auto riscv64_translation_context::reorder(Tuple tuple, std::index_sequence<idxs...>)
{
	return std::make_tuple(allocate_register(std::get<order[idxs]>(tuple)).first.reg()...);
}

template <reg_idx... idx>
std::tuple<to_type_t<reg_idx, idx, RegisterOperand>...> riscv64_translation_context::allocate_in_order(to_type_t<reg_idx, idx, const port *>... args)
{
	auto constexpr order = ordered<reg_idx, idx...>::value;

	auto tuple = std::make_tuple(args...);

	auto allocations = reorder<order>(tuple, std::make_index_sequence<sizeof...(idx)> {});

	return { (args && !args->targets().empty()) ? std::get<static_cast<const unsigned long>(idx) - flag_idx>(allocations) : builder::none_reg... };
}

/**
 * Used to get the register for the given port.
 * Will return the previously allocated TypedRegister or a new one with type set accordingly to the given port.
 * @param p Port of the register. Can be null for temporary.
 * @param reg1 Optional. Use this register for the lower half instead of allocating a new one.
 * @param reg2 Optional. Use this register for the upper half instead of allocating a new one.
 * @return A pair reference to the allocated Register and a boolean indicating whether the allocating was new.
 */
std::pair<TypedRegister &, bool> riscv64_translation_context::allocate_register(
	const port *p, std::optional<RegisterOperand> reg1, std::optional<RegisterOperand> reg2)
{
	if (p && treg_for_port_.count(p)) {
		TypedRegister &treg = treg_for_port_.at(p);
		if (!idxs_.empty()) {
			// We are inside a branch
			if (treg.type().width() <= 64) {
				if (treg.encoding() < idxs_.top()) {
					live_across_iteration_.push_front(treg.encoding());
				}
			} else {
				if (treg.encoding2() < idxs_.top()) {
					live_across_iteration_.push_front(treg.encoding1());
					live_across_iteration_.push_front(treg.encoding2());
				}
			}
		}
		return { treg, false };
	}
	if (!p) {
		RegisterOperand r1 = reg1 ? *reg1 : builder_.next_register();
		temporaries.emplace_front(r1);
		return { temporaries.front(), true };
	}

	switch (p->type().width()) {
	case 512: // FIXME proper
	case 128: {
		if (p->targets().size() > 1) {
			if (reg1) {
				RegisterOperand reg = builder_.next_register();
				builder_.mv(reg, *reg1);
				reg1 = reg;
			}
			if (reg2) {
				RegisterOperand reg = builder_.next_register();
				builder_.mv(reg, *reg2);
				reg2 = reg;
			}
		}

		RegisterOperand r1 = reg1 ? *reg1 : builder_.next_register();
		RegisterOperand r2 = reg2 ? *reg2 : builder_.next_register();
		auto [a, b] = treg_for_port_.emplace(std::piecewise_construct, std::forward_as_tuple(p), std::forward_as_tuple(r1, r2));
		TypedRegister &tr = a->second;
		tr.set_type(p->type());
		return { tr, true };
	}
	case 64:
	case 32:
	case 16:
	case 8:
	case 1: {
		if (p->targets().size() > 1) {
			if (reg1) {
				RegisterOperand reg = builder_.next_register();
				builder_.mv(reg, *reg1);
				reg1 = reg;
			}
		}
		RegisterOperand r1 = reg1 ? *reg1 : builder_.next_register();
		auto [a, b] = treg_for_port_.emplace(p, r1);
		TypedRegister &tr = a->second;
		tr.set_type(p->type());
		return { tr, true };
	}
	default:
		throw std::runtime_error("Invalid type for register allocation");
	}
}

RegisterOperand riscv64_translation_context::get_or_assign_mapped_register(uint32_t idx)
{
	if (idx >= static_cast<unsigned long>(reg_idx::RAX) && idx <= static_cast<unsigned long>(reg_idx::R15)) { // GPR
		uint32_t i = idx - 1;
		reg_written_[i] = reg_loaded_[i] = true; // Consider "assign" access as write
		RegisterOperand reg = RegisterOperand { RegisterOperand::FUNCTIONAL_BASE + i };
		if (!idxs_.empty()) {
			live_across_iteration_.push_front(reg.encoding());
		}
		return reg;
	} else { // FLAG
		uint32_t i = idx - flag_idx;
		flag_written_[i] = flag_loaded_[i] = true; // Consider "assign" access as write
		RegisterOperand reg = RegisterOperand { RegisterOperand::FUNCTIONAL_BASE + 16 + i };
		if (!idxs_.empty()) {
			live_across_iteration_.push_front(reg.encoding());
		}
		return reg;
	}
}

RegisterOperand riscv64_translation_context::get_or_load_mapped_register(uint32_t idx)
{
	if (idx >= static_cast<unsigned long>(reg_idx::RAX) && idx <= static_cast<unsigned long>(reg_idx::R15)) { // GPR
		uint32_t i = idx - 1;
		RegisterOperand reg = RegisterOperand { RegisterOperand::FUNCTIONAL_BASE + i };
		if (!reg_loaded_[i]) {
			reg_loaded_[i] = true;
			builder_.ld(reg, AddressOperand { FP, static_cast<intptr_t>(8 * idx) }); // FIXME hardcoded
		}
		if (!idxs_.empty()) {
			live_across_iteration_.push_front(reg.encoding());
		}
		return reg;
	} else { // FLAG
		uint32_t i = idx - flag_idx;
		RegisterOperand reg = RegisterOperand { RegisterOperand::FUNCTIONAL_BASE + 16 + i };
		if (!flag_loaded_[i]) {
			flag_loaded_[i] = true;
			builder_.lb(reg, AddressOperand { FP, static_cast<intptr_t>(flag_off + i) }); // FIXME hardcoded
		}
		if (!idxs_.empty()) {
			live_across_iteration_.push_front(reg.encoding());
		}
		return reg;
	}
}

void riscv64_translation_context::write_back_registers()
{
	for (uint32_t i = 8; i < reg_written_.size(); ++i) {
		if (reg_written_[i]) {
			builder_.sd(
				RegisterOperand { RegisterOperand::FUNCTIONAL_BASE + i }, AddressOperand { FP, static_cast<intptr_t>(8 * i + 8) }); // FIXME hardcoded offset
		}
	}

	for (uint32_t i = 0; i < flag_written_.size(); ++i) {
		if (flag_written_[i]) {
			builder_.sb(RegisterOperand { RegisterOperand::FUNCTIONAL_BASE + 16 + i }, AddressOperand { FP, static_cast<intptr_t>(flag_off + i) });
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
#ifndef NDEBUG
	builder_.li(ZERO, payload);
#endif
}

void riscv64_translation_context::begin_block()
{
	reg_loaded_ = 0x00ff;
	reg_written_.reset();
	flag_loaded_.reset();
	flag_written_.reset();
	builder_.reset();

#ifndef NDEBUG
	if (insert_ebreak) {
		builder_.ebreak();
	}
#endif

	add_marker(1);
}

void riscv64_translation_context::begin_instruction(off_t address, const std::string &disasm)
{
	add_marker(2);
	treg_for_port_.clear();
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
	for (size_t i = 0; i < std::size(v.flag_used_); ++i) {
		if (v.flag_used_[i]) {
			get_or_load_mapped_register(i + flag_idx);
		}
	}

	for (const auto &item : nodes_) {
		materialise(item.get());
	}

	for (const auto &item : locals_) {
		builder_.mv(ZERO, item.second.get());
	}

	add_marker(-2);
}

void riscv64_translation_context::end_block()
{
	// TODO Remove/only in debug
	if (insert_ebreak) {
		builder_.ebreak();
	}

	builder_.li(A0, ret_val_);
	builder_.ret();

	builder_.allocate();

	builder_.emit(assembler_);
}

void riscv64_translation_context::lower(const std::shared_ptr<action_node> &n)
{
	// Defer until end of block (when generation is finished)
	nodes_.push_back(n);
}

using load_store_func_t = decltype(&InstructionBuilder::ld);

static const std::unordered_map<std::size_t, load_store_func_t> load_instructions {
	{ 1, &InstructionBuilder::lb },
	{ 8, &InstructionBuilder::lb },
	{ 16, &InstructionBuilder::lh },
	{ 32, &InstructionBuilder::lw },
	{ 64, &InstructionBuilder::ld },
};

static const std::unordered_map<std::size_t, load_store_func_t> store_instructions {
	{ 1, &InstructionBuilder::sb },
	{ 8, &InstructionBuilder::sb },
	{ 16, &InstructionBuilder::sh },
	{ 32, &InstructionBuilder::sw },
	{ 64, &InstructionBuilder::sd },
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
		if (!locals_.count(node.local())) {
			TypedRegister &local = allocate_register().first;
			locals_.emplace(node.local(), std::ref(local));
			// First write to local (used register free for use before this)
			builder_.mv(local, write_reg);
		} else {
			auto &reg_enc = locals_.at(node.local()).get();
			// Successive write, keep available
			builder_.mv_keep(reg_enc, write_reg);
		}
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

	auto [zf, of, cf, sf] = allocate_in_order<reg_idx::ZF, reg_idx::OF, reg_idx::CF, reg_idx::SF>(&n.zero(), &n.overflow(), &n.carry(), &n.negative());
	bool flags_needed = zf || of || cf || sf;

	Label *fail = builder_.alloc_label();
	Label *retry = builder_.alloc_label();
	Label *end = builder_.alloc_label();

	using load_reserve_type = decltype(&InstructionBuilder::lrd);
	using store_conditional_type = decltype(&InstructionBuilder::scd);

	load_reserve_type lr = n.val().type().element_width() == 32 ? &InstructionBuilder::lrw : &InstructionBuilder::lrd;
	store_conditional_type sc = n.val().type().element_width() == 32 ? &InstructionBuilder::scw : &InstructionBuilder::scd;
	load_store_func_t store = n.val().type().element_width() == 32 ? &InstructionBuilder::sw : &InstructionBuilder::sd;

	auto addr = AddressOperand { dstAddr };
	// FIXME Correct memory ordering?
	switch (n.op()) {

	case ternary_atomic_op::cmpxchg:
		switch (n.rhs().type().element_width()) {
		case 64:
		case 32: {
			auto &temp = allocate_register().first;
			builder_.Bind(retry);

			(builder_.*lr)(out_reg, addr, std::memory_order_acq_rel);
			builder_.bne(out_reg, acc, fail, Assembler::kNearJump);
			(builder_.*sc)(temp, src, addr, std::memory_order_acq_rel); // Out_reg unused so use it here
			builder_.bnez(temp, retry, Assembler::kNearJump);

			// Keep live across iterations
			builder_.mv(ZERO, src);
			builder_.mv(ZERO, addr.base());
			builder_.mv(ZERO, acc);

			// Flags from comparison matching (i.e. subtraction of equal values)
			if (zf) {
				builder_.li(zf, 1);
			}
			if (cf) {
				builder_.li(cf, 0);
			}
			if (of) {
				builder_.li(of, 0);
			}
			if (sf) {
				builder_.li(sf, 0);
			}

			builder_.j(end, Assembler::kNearJump);

			builder_.Bind(fail);

			if (flags_needed) {
				auto &temp_reg = allocate_register().first;
				temp_reg.set_type(n.val().type());
				sub(builder_, temp_reg, acc, out_reg);
				sub_flags(builder_, temp_reg, acc, out_reg, zf, of, cf, sf);
			}

			// Write back updated acc value
			builder_.mv(get_or_assign_mapped_register(reinterpret_cast<read_reg_node *>(n.rhs().owner())->regidx()), out_reg);

			builder_.Bind(end);
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
	auto [zf, of, cf, sf] = allocate_in_order<reg_idx::ZF, reg_idx::OF, reg_idx::CF, reg_idx::SF>(&n.zero(), &n.overflow(), &n.carry(), &n.negative());
	bool flags_needed = zf || of || cf || sf;

	auto addr = AddressOperand { dstAddr };

	auto &temp_result_reg = allocate_register().first;

	// FIXME Correct memory ordering?
	switch (n.op()) {
	case binary_atomic_op::xadd:
	case binary_atomic_op::add: {
		switch (n.val().type().element_width()) {
		case 64:
			builder_.amoaddd(out_reg, src, addr, std::memory_order_acq_rel);
			break;
		case 32:
			builder_.amoaddw(out_reg, src, addr, std::memory_order_acq_rel);
			break;
		default:
			throw std::runtime_error("unsupported xadd width");
		}

		if (flags_needed) {
			temp_result_reg.set_type(n.val().type());

			add(builder_, temp_result_reg, out_reg, src); // Actual sum for flag generation
			add_flags(builder_, temp_result_reg, out_reg, src, zf, of, cf, sf);
		}

		if (n.op() == binary_atomic_op::xadd) {
			//			Not needed anymore
			//			switch (n.val().type().element_width()) { // FIXME This feels so wrong
			//			case 32:
			//				assembler_.slli(out_reg, out_reg, 32);
			//				assembler_.srli(out_reg, out_reg, 32);
			//				[[fallthrough]];
			//			case 64:
			//				assembler_.mv(get_or_assign_mapped_register(reinterpret_cast<read_reg_node *>(n.rhs().owner())->regidx()), out_reg);
			//				break;
			//			default:
			//				throw std::runtime_error("unsupported xadd width");
			//			}
			return out_reg;
		}
		return std::nullopt;
	}
	case binary_atomic_op::sub:
		builder_.neg(out_reg, src);
		switch (n.val().type().element_width()) {
		case 64:
			builder_.amoaddd(out_reg, out_reg, addr, std::memory_order_acq_rel);
			break;
		case 32:
			builder_.amoaddw(out_reg, src, addr, std::memory_order_acq_rel);
			break;
		default:
			throw std::runtime_error("unsupported lock sub width");
		}

		if (flags_needed) {
			temp_result_reg.set_type(n.val().type());

			sub(builder_, temp_result_reg, out_reg, src); // Actual difference for flag generation
			sub_flags(builder_, temp_result_reg, out_reg, src, zf, of, cf, sf);
		}
		return std::nullopt;
	case binary_atomic_op::band: {
		switch (n.val().type().element_width()) {
		case 64:
			builder_.amoandd(out_reg, src, addr, std::memory_order_acq_rel);
			if (flags_needed) {
				builder_.and_(temp_result_reg, out_reg, src); // Actual and for flag generation
			}
			break;
		case 32:
			builder_.amoandw(out_reg, src, addr, std::memory_order_acq_rel);
			if (flags_needed) {
				builder_.and_(temp_result_reg, out_reg, src); // Actual and for flag generation
				builder_.slli(temp_result_reg, temp_result_reg, 32); // Get rid of higher 32 bits
			}
			break;
		default:
			throw std::runtime_error("unsupported lock and width");
		}
		zero_sign_flag(builder_, temp_result_reg, zf, sf);
		return std::nullopt;
	}
	case binary_atomic_op::bor: {
		switch (n.val().type().element_width()) {
		case 64:
			builder_.amoord(out_reg, src, addr, std::memory_order_acq_rel);
			if (flags_needed) {
				builder_.or_(temp_result_reg, out_reg, src); // Actual or for flag generation
			}
			break;
		case 32:
			builder_.amoorw(out_reg, src, addr, std::memory_order_acq_rel);
			if (flags_needed) {
				builder_.or_(temp_result_reg, out_reg, src); // Actual or for flag generation
				builder_.slli(temp_result_reg, temp_result_reg, 32); // Get rid of higher 32 bits
			}
			break;
		default:
			throw std::runtime_error("unsupported lock or width");
		}

		zero_sign_flag(builder_, temp_result_reg, zf, sf);
		return std::nullopt;
	}
	case binary_atomic_op::bxor: {
		switch (n.val().type().element_width()) {
		case 64:
			builder_.amoxord(out_reg, src, addr, std::memory_order_acq_rel);
			if (flags_needed) {
				builder_.xor_(temp_result_reg, out_reg, src); // Actual xor for flag generation
			}
			break;
		case 32:
			builder_.amoxorw(out_reg, src, addr, std::memory_order_acq_rel);

			if (flags_needed) {
				builder_.xor_(temp_result_reg, out_reg, src); // Actual xor for flag generation
				builder_.slli(temp_result_reg, temp_result_reg, 32); // Get rid of higher 32 bits
			}
			break;
		default:
			throw std::runtime_error("unsupported lock xor width");
		}

		zero_sign_flag(builder_, temp_result_reg, zf, sf);
		return std::nullopt;
	}
	case binary_atomic_op::xchg:
		switch (n.val().type().element_width()) {
		case 64:
			builder_.amoswapd(out_reg, src, addr, std::memory_order_acq_rel);
			break;
		case 32:
			builder_.amoswapw(out_reg, src, addr, std::memory_order_acq_rel);
			break;
		default:
			throw std::runtime_error("unsupported xchg width");
		}

		switch (n.val().type().element_width()) {
		case 32:
			builder_.slli(out_reg, out_reg, 32);
			builder_.srli(out_reg, out_reg, 32);
			[[fallthrough]];
		case 64:
			builder_.mv(get_or_assign_mapped_register(reinterpret_cast<read_reg_node *>(n.rhs().owner())->regidx()), out_reg);
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
			fixup(builder_, src_reg, src_reg, value_type::s64());
		}
		fixup(builder_, src_reg, src_reg, n.val().type().get_signed_type());
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
				fixup(builder_, out_reg, src_reg, n.val().type().get_unsigned_type());
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
		fixup(builder_, out_reg, src_reg, n.val().type().get_unsigned_type());
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

		truncate(builder_, out_reg, src_reg);
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

	bit_extract(builder_, out_reg, src, from, length);

	return out_reg;
}

TypedRegister &riscv64_translation_context::materialise_bit_insert(const bit_insert_node &n)
{
	int to = n.to();
	int length = n.length();

	TypedRegister &src = *materialise(n.source_value().owner());

	TypedRegister &bits = *materialise(n.bits().owner());

	if (is_i128(n.val()) && is_i128(n.source_value()) && is_gpr(n.bits())) {
		if (to == 0 && length == 64) {
			// Only map don't modify registers
			auto [out_reg, _] = allocate_register(&n.val(), bits, src.reg2());
			return out_reg;
		} else if (to == 64 && length == 64) {
			// Only map don't modify registers
			auto [out_reg, _] = allocate_register(&n.val(), src.reg1(), bits);
			return out_reg;
		} else if (to + length <= 64) {
			// Map upper and insert into lower
			auto [out_reg, valid] = allocate_register(&n.val(), std::nullopt, src.reg2());
			if (valid) {
				bit_insert(builder_, out_reg, src, bits, to, length);
			}
			return out_reg;
		} else if (to >= 64) {
			// Map lower and insert into upper
			auto [out_reg, valid] = allocate_register(&n.val(), src.reg1());
			if (valid) {
				bit_insert(builder_, out_reg, src, bits, to, length);
			}
			return out_reg;
		} else {
			throw std::runtime_error("Unsupported bit insert arguments for 128 bit width.");
		}
	} else if (is_gpr(n.val()) && is_gpr(n.source_value()) && is_gpr(n.bits())) {
		// Insert into 64B register
		auto [out_reg, valid] = allocate_register(&n.val());
		if (valid) {
			bit_insert(builder_, out_reg, src, bits, to, length);
		}
		return out_reg;
	} else {
		throw std::runtime_error("Unsupported bit insert width.");
	}
}

TypedRegister &riscv64_translation_context::materialise_read_reg(const read_reg_node &n)
{
	const port &value = n.val();
	if (value.targets().size() == 1
		&& ((is_int(value, 64) && n.regidx() <= static_cast<unsigned long>(reg_idx::R15)) || is_flag(value))) { // 64bit GPR or flag only used once
		Register reg = get_or_load_mapped_register(n.regidx());
		TypedRegister &out_reg = allocate_register(&n.val(), reg).first;
		if (is_flag(value)) {
			out_reg.set_type(value_type::u64());
		}
		return out_reg;
	}

	auto [out_reg, valid] = allocate_register(&n.val());
	if (!valid) {
		return out_reg;
	}
	if (is_gpr(value)) {
		if (n.regidx() > static_cast<unsigned long>(reg_idx::R15)) { // Not GPR
			auto load_instr = load_instructions.at(value.type().element_width());
			(builder_.*load_instr)(out_reg, AddressOperand { FP, static_cast<intptr_t>(n.regoff()) });
			out_reg.set_actual_width();
			out_reg.set_type(value_type::u64());
		} else {
			RegisterOperand reg = get_or_load_mapped_register(n.regidx());
			if (is_int(value, 32)) {
				builder_.sextw(out_reg, reg);
				out_reg.set_actual_width();
				out_reg.set_type(value_type::u64());
			} else {
				builder_.mv(out_reg, reg);
			}
		}

		return out_reg;
	}
	if (is_flag(value)) {
		out_reg.set_type(value_type::u64());
		RegisterOperand reg = get_or_load_mapped_register(n.regidx());
		builder_.mv(out_reg, reg);
		return out_reg;
	} else if (is_i128(value) || is_int(value, 512)) {
		builder_.ld(out_reg.reg1(), AddressOperand { FP, static_cast<intptr_t>(n.regoff()) });
		builder_.ld(out_reg.reg2(), AddressOperand { FP, static_cast<intptr_t>(n.regoff() + 8) });
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
		if (n.regidx() > static_cast<unsigned long>(reg_idx::R15) || n.regidx() < static_cast<unsigned long>(reg_idx::RAX)) { // Not GPR
			auto store_instr = store_instructions.at(value.type().element_width());
			(builder_.*store_instr)(reg, AddressOperand { FP, static_cast<intptr_t>(n.regoff()) });
		} else {
			builder_.mv(get_or_assign_mapped_register(n.regidx()), reg);
		}
		return;
	} else if (is_flag(value)) {
		TypedRegister &reg_in = *(materialise(value.owner()));
		RegisterOperand reg = (!is_flag_port(value)) ? reg_in : reg_in.flag(n.regidx() - flag_idx);
		builder_.mv(get_or_assign_mapped_register(n.regidx()), reg);
		return;
	} else if (is_i128(value) || is_int_vector(value, 2, 64) || is_int_vector(value, 4, 32) || is_int(value, 512) || is_int_vector(value, 4, 128)) {
		// Treat 512 as 128 for now. Assuming it is just 128 bit instructions acting on 512 registers
		TypedRegister &reg = *(materialise(value.owner())); // Will give lower 64bit

		builder_.sd(reg.reg1(), AddressOperand { FP, static_cast<intptr_t>(n.regoff()) });
		builder_.sd(reg.reg2(), AddressOperand { FP, static_cast<intptr_t>(n.regoff() + 8) });
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

	AddressOperand addr { addr_reg };

	if (is_i128(n.val())) {
		builder_.ld(out_reg.reg1(), addr);
		builder_.ld(out_reg.reg2(), AddressOperand { addr_reg, 8 });
		return out_reg;
	}

	if (!(is_gpr(n.val()) && is_gpr(n.address()))) {
		throw std::runtime_error("unsupported width on read mem operation");
	}

	auto load_instr = load_instructions.at(n.val().type().element_width());
	(builder_.*load_instr)(out_reg, addr);
	out_reg.set_actual_width();
	out_reg.set_type(value_type::u64());

	return out_reg;
}

void riscv64_translation_context::materialise_write_mem(const write_mem_node &n)
{
	TypedRegister &src_reg = *materialise(n.value().owner());
	TypedRegister &addr_reg = *materialise(n.address().owner());

	AddressOperand addr { addr_reg };

	if (is_i128(n.value())) {
		builder_.sd(src_reg.reg1(), addr);
		builder_.sd(src_reg.reg2(), AddressOperand { addr_reg, 8 });
		return;
	}

	if (!(is_gpr(n.value()) && is_gpr(n.address()))) {
		throw std::runtime_error("unsupported width on write mem operation");
	}

	auto store_instr = store_instructions.at(n.value().type().element_width());
	(builder_.*store_instr)(src_reg, addr);
}

TypedRegister &riscv64_translation_context::materialise_read_pc(const read_pc_node &n) { return materialise_constant(current_address_); }

void riscv64_translation_context::materialise_write_pc(const write_pc_node &n)
{
	if (!is_gpr(n.value())) {
		throw std::runtime_error("unsupported width on write pc operation");
	}

	if (n.updates_pc() == br_type::call)
		ret_val_ = 3;
	if (n.updates_pc() == br_type::ret)
		ret_val_ = 4;

	if (ret_val_ == 0) { // Only chain on normal block end
		const std::optional<int64_t> &target = get_as_int(n.value().owner());

		if (target) { // Unconditional direct jump or call
			// Set up chain

			// Write back all registers
			write_back_registers();
			// Now A1 register is available

			// Save address to patch jump into when chaining (A1 second ret value)
			builder_.auipc(A1, 0);

			TypedRegister &reg = materialise_constant(*target);
			builder_.sd(reg, AddressOperand { FP, static_cast<intptr_t>(reg_offsets::PC) });
			return;
		} else if (n.value().owner()->kind() == node_kinds::csel) {
			const auto &node = *reinterpret_cast<const csel_node *>(n.value().owner());
			const std::optional<int64_t> &target1 = get_as_int(node.falseval().owner());
			const std::optional<int64_t> &target2 = get_as_int(node.trueval().owner());
			if (target1 && target2) { // Conditional direct jump
				TypedRegister &cond = *materialise(node.condition().owner());
				extend_to_64(builder_, cond, cond);

				// Set up chain

				// Write back all registers
				write_back_registers();
				// Now A1 register is available

				// Save address to patch jump into when chaining (A1 second ret value)
				builder_.auipc(A1, 0);

				Label *false_calc = builder_.alloc_label();
				Label *end = builder_.alloc_label();

				builder_.beqz(cond, false_calc, Assembler::kNearJump);

				builder_.Align(Assembler::label_align);
				builder_.auipc_keep(A1, 0);
				TypedRegister &trueval = materialise_constant(*target2);
				builder_.sd(trueval, AddressOperand { FP, static_cast<intptr_t>(reg_offsets::PC) });
				builder_.j(end, Assembler::kNearJump);

				builder_.Bind(false_calc);

				TypedRegister &falseval = materialise_constant(*target1);
				builder_.sd(falseval, AddressOperand { FP, static_cast<intptr_t>(reg_offsets::PC) });

				builder_.Bind(end);
				return;
			}
		}
	}
	// Indirect jump or call or return => No chain
	// FIXME maybe only support 64bit values for PC
	TypedRegister &src_reg = *materialise(n.value().owner());

	AddressOperand addr { FP, static_cast<intptr_t>(0) }; // FIXME hardcode
	auto store_instr = store_instructions.at(n.value().type().element_width());
	(builder_.*store_instr)(src_reg, addr);

	// Write back all registers
	write_back_registers();

	builder_.li(A1, 0); // 0 = No chain
}

void riscv64_translation_context::materialise_label(const label_node &n)
{
	auto [it, not_exist] = labels_.try_emplace(&n, std::make_pair(nullptr, true));

	if (not_exist) {
		it->second.first = builder_.alloc_label();
		// BW label
		idxs_.push(builder_.next_register().encoding());
	} else {
		it->second.second = true;
	}
	builder_.Bind(it->second.first);
}

inline void riscv64_translation_context::bw_branch_vreg_helper(bool bw)
{
	if (bw) {
		idxs_.pop();
		for (auto it = live_across_iteration_.cbefore_begin(), it_next = std::next(it); it_next != live_across_iteration_.cend();) {
			const RegisterOperand elem = RegisterOperand { *it_next };
			if (idxs_.empty() || (elem.encoding() >= RegisterOperand::VIRTUAL_BASE && idxs_.top() < elem.encoding())) {
				live_across_iteration_.erase_after(it);
			} else {
				++it;
			}
			it_next = std::next(it);
			builder_.mv(ZERO, elem);
		}
	}
}

void riscv64_translation_context::materialise_br(const br_node &n)
{
	auto [it, not_exist] = labels_.try_emplace(n.target(), std::make_pair(nullptr, false));

	if (not_exist) {
		it->second.first = builder_.alloc_label();
	}
	builder_.j(it->second.first);
	bw_branch_vreg_helper(it->second.second);
}

void riscv64_translation_context::materialise_cond_br(const cond_br_node &n)
{
	TypedRegister &cond = *materialise(n.cond().owner());

	extend_to_64(builder_, cond, cond);

	auto [it, not_exist] = labels_.try_emplace(n.target(), std::make_pair(nullptr, false));

	if (not_exist) {
		it->second.first = builder_.alloc_label();
	}
	builder_.bnez(cond, it->second.first);
	bw_branch_vreg_helper(it->second.second);
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
		not_(builder_, out_reg, src_reg);
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

	auto [zf, of, cf, sf] = allocate_in_order<reg_idx::ZF, reg_idx::OF, reg_idx::CF, reg_idx::SF>(&n.zero(), &n.overflow(), &n.carry(), &n.negative());
	bool flags_needed = zf || of || cf || sf;

	RegisterOperand tmp_carry = cf ? allocate_register().first : builder::none_reg;
	RegisterOperand tmp_overflow = of ? allocate_register().first : builder::none_reg;

	TypedRegister &src_reg1 = *materialise(n.lhs().owner());
	TypedRegister &src_reg2 = *materialise(n.rhs().owner());
	TypedRegister &src_reg3 = *materialise(n.top().owner());

	auto &temp_result = allocate_register().first;

	switch (n.op()) {
		// TODO Immediate handling

	case ternary_arith_op::adc:
		add(builder_, temp_result, src_reg2, src_reg3);
		addi_flags(builder_, temp_result, src_reg2, 1, builder::none_reg, tmp_overflow, tmp_carry, builder::none_reg);
		add(builder_, out_reg, src_reg1, temp_result);
		add_flags(builder_, out_reg, src_reg1, temp_result, builder::none_reg, of, cf, builder::none_reg);
		if (cf) {
			builder_.or_(cf, cf, tmp_carry); // Total carry out
		}
		if (of) {
			builder_.xor_(of, of, tmp_overflow); // Total overflow out
		}
		zero_sign_flag(builder_, out_reg, zf, sf);
		break;
	case ternary_arith_op::sbb:
		add(builder_, temp_result, src_reg2, src_reg3);
		addi_flags(builder_, temp_result, src_reg2, 1, builder::none_reg, tmp_overflow, tmp_carry, builder::none_reg);
		sub(builder_, out_reg, src_reg1, temp_result);
		sub_flags(builder_, out_reg, src_reg1, temp_result, builder::none_reg, of, cf, builder::none_reg);
		if (cf) {
			builder_.or_(cf, cf, tmp_carry); // Total carry out
		}
		if (of) {
			builder_.xor_(of, of, tmp_overflow); // Total overflow out
		}
		zero_sign_flag(builder_, out_reg, zf, sf);
		break;
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
	auto [zf, _1, _2, sf] = allocate_in_order<reg_idx::ZF, reg_idx::OF, reg_idx::CF, reg_idx::SF>(&n.zero(), nullptr, nullptr, &n.negative());

	TypedRegister &src_reg = *materialise(n.input().owner());
	const std::optional<int64_t> &i = get_as_int(n.amount().owner());

	if (i) {
		auto amt = *i;
		if (amt == 0) {
			return src_reg;
		}
		switch (n.op()) {
		case shift_op::lsl:
			slli(builder_, out_reg, src_reg, amt);
			break;
		case shift_op::lsr:
			srli(builder_, out_reg, src_reg, amt);
			break;
		case shift_op::asr:
			srai(builder_, out_reg, src_reg, amt);
			break;
		}
		zero_sign_flag(builder_, out_reg, zf, sf);

		return out_reg;
	}

	auto amount = *materialise(n.amount().owner());

	switch (n.op()) {
	case shift_op::lsl:
		sll(builder_, out_reg, src_reg, amount);
		break;
	case shift_op::lsr:
		srl(builder_, out_reg, src_reg, amount);
		break;
	case shift_op::asr:
		sra(builder_, out_reg, src_reg, amount);
		break;
	}
	zero_sign_flag(builder_, out_reg, zf, sf);

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
	auto [zf, of, cf, sf] = allocate_in_order<reg_idx::ZF, reg_idx::OF, reg_idx::CF, reg_idx::SF>(&n.zero(), &n.overflow(), &n.carry(), &n.negative());

	TypedRegister &src_reg1 = *materialise(n.lhs().owner());

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
				addi(builder_, out_reg, src_reg1, -imm);
				subi_flags(builder_, out_reg, src_reg1, imm, zf, of, cf, sf);
				break;
			case binary_arith_op::add:
				addi(builder_, out_reg, src_reg1, imm);
				addi_flags(builder_, out_reg, src_reg1, imm, zf, of, cf, sf);
				break;

			// Binary operations preserve sign extension, so we keep the effective input type (even if larger)
			case binary_arith_op::band:
				builder_.andi(out_reg, src_reg1, imm);
				out_reg.set_actual_width();
				out_reg.set_type(src_reg1.type());
				zero_sign_flag(builder_, out_reg, zf, sf);
				break;
			case binary_arith_op::bor:
				builder_.ori(out_reg, src_reg1, imm);
				out_reg.set_actual_width();
				out_reg.set_type(src_reg1.type());
				zero_sign_flag(builder_, out_reg, zf, sf);
				break;
			case binary_arith_op::bxor:
				builder_.xori(out_reg, src_reg1, imm);
				out_reg.set_actual_width();
				out_reg.set_type(src_reg1.type());
				zero_sign_flag(builder_, out_reg, zf, sf);
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
		add(builder_, out_reg, src_reg1, src_reg2);
		add_flags(builder_, out_reg, src_reg1, src_reg2, zf, of, cf, sf);
		break;
	case binary_arith_op::sub:
		sub(builder_, out_reg, src_reg1, src_reg2);
		sub_flags(builder_, out_reg, src_reg1, src_reg2, zf, of, cf, sf);
		break;
	// Binary operations preserve sign extension, so we can keep smaller of effective input types
	case binary_arith_op::band:
		builder_.and_(out_reg, src_reg1, src_reg2);
		out_reg.set_actual_width();
		out_reg.set_type(get_minimal_type(src_reg1, src_reg2));
		zero_sign_flag(builder_, out_reg, zf, sf);
		break;
	case binary_arith_op::bor:
		builder_.or_(out_reg, src_reg1, src_reg2);
		out_reg.set_actual_width();
		out_reg.set_type(get_minimal_type(src_reg1, src_reg2));
		zero_sign_flag(builder_, out_reg, zf, sf);
		break;
	case binary_arith_op::bxor:
		xor_(builder_, out_reg, src_reg1, src_reg2);
		zero_sign_flag(builder_, out_reg, zf, sf);
		break;
	case binary_arith_op::mul:
		// FIXME
		if (n.val().type().element_width() == 128 && (n.lhs().owner()->kind() != node_kinds::cast || n.rhs().owner()->kind() != node_kinds::cast)) {
			throw std::runtime_error("128bit multiply without cast");
		}
		mul(builder_, out_reg, src_reg1, src_reg2);
		mul_flags(builder_, out_reg, of, cf);
		break;
	case binary_arith_op::div:
		div(builder_, out_reg, src_reg1, src_reg2);
		break;
	case binary_arith_op::mod:
		mod(builder_, out_reg, src_reg1, src_reg2);
		break;
	case binary_arith_op::cmpeq: {
		RegisterOperand tmp = static_cast<const RegisterOperand>(src_reg2) != ZERO ? out_reg : src_reg1;
		if (static_cast<const RegisterOperand>(src_reg2) != ZERO) {
			builder_.xor_(out_reg, src_reg1, src_reg2);
		}
		builder_.seqz(out_reg, tmp);
		out_reg.set_actual_width(src_reg1.actual_width() < src_reg2.actual_width() ? src_reg1.actual_width_0() : src_reg2.actual_width_0());
		out_reg.set_type(get_minimal_type(src_reg1, src_reg2));
		return out_reg;
	}
	case binary_arith_op::cmpne: {
		RegisterOperand tmp = static_cast<const RegisterOperand>(src_reg2) != ZERO ? out_reg : src_reg1;
		if (static_cast<const RegisterOperand>(src_reg2) != ZERO) {
			builder_.xor_(out_reg, src_reg1, src_reg2);
		}
		builder_.snez(out_reg, tmp);
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
	gen_constant(builder_, imm, reg);
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

	Label *false_calc = builder_.alloc_label();
	Label *end = builder_.alloc_label();

	TypedRegister &cond = *materialise(n.condition().owner());

	extend_to_64(builder_, cond, cond);

	TypedRegister &trueval = *materialise(n.trueval().owner());

	TypedRegister &falseval = *materialise(n.falseval().owner());

	builder_.beqz(cond, false_calc); // TODO Single instruction jump optimization

	builder_.mv(out_reg, trueval);

	builder_.j(end);

	builder_.Bind(false_calc);

	builder_.mv_keep(out_reg, falseval);

	builder_.Bind(end);

	// In-types might be wider than out-type so out accurate to narrower of the two
	out_reg.set_type(get_minimal_type(trueval, falseval));
	out_reg.set_actual_width(trueval.actual_width() < falseval.actual_width() ? trueval.actual_width_0() : falseval.actual_width_0());
	return out_reg;
}

void riscv64_translation_context::materialise_internal_call(const internal_call_node &n)
{
	write_back_registers();
	assembler_.li(A1, 0);

	const auto &function = n.fn();
	if (function.name() == "handle_syscall") {
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
			return allocate_register(&n.val(), insert, src.reg2()).first;
		} else if (n.index() == 1) {
			// No value modification just map correctly
			return allocate_register(&n.val(), src.reg1(), insert).first;
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
				bit_insert(builder_, out_reg, src, insert, 32 * n.index(), 32);
			}
			return out_reg;
		}
		case 2:
		case 3: {
			auto [out_reg, valid] = allocate_register(&n.val(), src.reg1(), std::nullopt);
			if (valid) {
				bit_insert(builder_, out_reg, src, insert, 32 * n.index(), 32);
			}
			return out_reg;
		}
		default:
			throw std::runtime_error("Unsupported vector insert index");
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

		bit_extract(builder_, out_reg, src, 32 * n.index(), 32);
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
			chain_machine_code_writer writer = chain_machine_code_writer { instr_p, 12 };

			Assembler ass { &writer, false, RV_GC };

			intptr_t offset = ass.offset_from_target(reinterpret_cast<intptr_t>(chain_target));

			if (!IsBTypeImm(offset)) {
				Label end {};

				if (following_instr.opcode() == C_BEQZ) {
					ass.bnez(following_instr.rs1p(), &end, Assembler::kNearJump);
				} else {
					ass.beqz(following_instr.rs1p(), &end, Assembler::kNearJump);
				}

				offset = ass.offset_from_target(reinterpret_cast<intptr_t>(chain_target));
				ass.j(offset);
				ass.Align(Assembler::label_align);
				ass.Bind(&end);
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
			chain_machine_code_writer writer
				= *(instr_p - 1) == 0b10011 ? chain_machine_code_writer { instr_p - 1, 16 } : chain_machine_code_writer { instr_p, 16 };

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
			chain_machine_code_writer writer = chain_machine_code_writer { instr_p, 16 };

			Assembler ass { &writer, false, RV_GC };

			intptr_t offset = ass.offset_from_target(reinterpret_cast<intptr_t>(chain_target));

			if (!IsBTypeImm(offset)) {
				Label end {};
				switch (following_instr.funct3()) {
				case BEQ:
					ass.bne(following_instr.rs1(), following_instr.rs2(), &end);
					break;
				case BNE:
					ass.beq(following_instr.rs1(), following_instr.rs2(), &end);
					break;
				case BLT:
					ass.bge(following_instr.rs1(), following_instr.rs2(), &end);
					break;
				case BGE:
					ass.blt(following_instr.rs1(), following_instr.rs2(), &end);
					break;
				case BLTU:
					ass.bgeu(following_instr.rs1(), following_instr.rs2(), &end);
					break;
				case BGEU:
					ass.bltu(following_instr.rs1(), following_instr.rs2(), &end);
					break;
				default:
					throw std::runtime_error("Should not happen");
				}

				offset = ass.offset_from_target(reinterpret_cast<intptr_t>(chain_target));
				ass.j(offset);
				ass.Align(Assembler::label_align);
				ass.Bind(&end);
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
			chain_machine_code_writer writer
				= *(instr_p - 1) == 0b10011 ? chain_machine_code_writer { instr_p - 1, 16 } : chain_machine_code_writer { instr_p, 16 };

			Assembler ass { &writer, false, RV_GC };
			intptr_t offset = ass.offset_from_target(reinterpret_cast<intptr_t>(chain_target));

			if (!IsJTypeImm(offset)) {
				throw std::runtime_error("Chaining failed. Jump offset too big for single direct jump instruction.");
			}
			ass.j(offset);
		}
	}
}
