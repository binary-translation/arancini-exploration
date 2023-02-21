#include <arancini/ir/node.h>
#include <arancini/output/dynamic/riscv64/encoder/riscv64-constants.h>
#include <arancini/output/dynamic/riscv64/riscv64-translation-context.h>

#include <functional>
#include <unordered_map>

using namespace arancini::output::dynamic::riscv64;
using namespace arancini::ir;

/**
 * All values are always held sign extended to 64 bits in RISCV registers for simple flag calculation
 * Translations assumes FP holds pointer to CPU state.
 * Flags are always stored in registers S8 (ZF), S9 (CF), S10 (OF), S11(SF)
 */
constexpr Register ZF = S8;
constexpr Register CF = S9;
constexpr Register OF = S10;
constexpr Register SF = S11;

void riscv64_translation_context::begin_block() { }
void riscv64_translation_context::begin_instruction(off_t address, const std::string &disasm) { }
void riscv64_translation_context::end_instruction() { }
void riscv64_translation_context::end_block() { }
void riscv64_translation_context::lower(ir::node *n) { materialise(n); }

static inline bool is_flag(const port& value) { return value.type().width() == 1; }
static inline bool is_gpr(const port& value) { return value.type().width() == 64; }

using load_store_func_t = decltype(&Assembler::ld);

static std::unordered_map<std::size_t, load_store_func_t>
load_instructions {
    {8, &Assembler::lb},
    {16, &Assembler::lh},
    {32, &Assembler::lw},
    {64, &Assembler::ld},
};

static std::unordered_map<std::size_t, load_store_func_t>
store_instructions {
    {8, &Assembler::sb},
    {16, &Assembler::sh},
    {32, &Assembler::sw},
    {64, &Assembler::sd},
};

std::variant<Register, std::unique_ptr<Label>, std::monostate>
riscv64_translation_context::materialise(const node *n) {
    if (!n)
        throw std::runtime_error("RISC-V DBT received NULL pointer to node");

	switch (n->kind()) {
	case node_kinds::bit_shift:
		return materialise_bit_shift(*reinterpret_cast<const bit_shift_node *>(n));
	case node_kinds::read_reg:
        return materialise_read_reg(*reinterpret_cast<const read_reg_node*>(n));
	case node_kinds::write_reg:
        materialise_write_reg(*reinterpret_cast<const write_reg_node*>(n));
        return std::monostate{};
	case node_kinds::read_mem:
        return materialise_read_mem(*reinterpret_cast<const read_mem_node*>(n));
	case node_kinds::write_mem:
        materialise_write_mem(*reinterpret_cast<const write_mem_node*>(n));
        return std::monostate{};
	case node_kinds::read_pc:
        return materialise_read_pc(*reinterpret_cast<const read_pc_node*>(n));
	case node_kinds::write_pc:
        materialise_write_pc(*reinterpret_cast<const write_pc_node*>(n));
        return std::monostate{};
    case node_kinds::label:
        return materialise_label(*reinterpret_cast<const label_node*>(n));
    case node_kinds::br:
        materialise_br(*reinterpret_cast<const br_node*>(n));
        return std::monostate{};
    case node_kinds::cond_br:
        materialise_cond_br(*reinterpret_cast<const cond_br_node*>(n));
        return std::monostate{};
	case node_kinds::constant:
		return materialise_constant((int64_t)((constant_node *)n)->const_val_i());
	case node_kinds::binary_arith:
		return materialise_binary_arith(*reinterpret_cast<const binary_arith_node *>(n));
	case node_kinds::unary_arith:
        return materialise_unary_arith(*reinterpret_cast<const unary_arith_node*>(n));
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
	default:
		throw std::runtime_error("unsupported node");
	}
}
Register riscv64_translation_context::materialise_ternary_atomic(const ternary_atomic_node &n)
{

	Register dstAddr = std::get<Register>(materialise(n.lhs().owner()));
	Register acc = std::get<Register>(materialise(n.rhs().owner()));
	Register src = std::get<Register>(materialise(n.top().owner()));
	Register out_reg = T0;
	Label fail;
	Label retry;
	Label end;
	// FIXME Correct memory ordering?
	switch (n.op()) {

	case ternary_atomic_op::cmpxchg:
		switch (n.rhs().type().width()) {
		case 64:
			assembler_.Bind(&retry);

			assembler_.lrd(out_reg, Address { dstAddr }, std::memory_order_seq_cst);
			assembler_.bne(out_reg, acc, &fail, Assembler::kNearJump);
			assembler_.scd(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);
			assembler_.bnez(out_reg, &retry, Assembler::kNearJump);

			// Flags from comparison matching (i.e subtraction of equal values)
			assembler_.li(CF, 0);
			assembler_.li(OF, 0);
			assembler_.li(SF, 0);
			assembler_.li(ZF, 1);

			assembler_.j(&end, Assembler::kNearJump);

			assembler_.Bind(&fail);

			assembler_.sub(SF, acc, out_reg);

			assembler_.sgtz(CF, out_reg);
			assembler_.slt(OF, SF, acc);
			assembler_.xor_(OF, OF, CF); // OF

			assembler_.sltu(CF, acc, SF); // CF

			assembler_.li(ZF, 0); // ZF

			assembler_.sltz(SF, SF); // SF

			//Write back updated acc value
			assembler_.sd(out_reg, { FP, static_cast<intptr_t>(reinterpret_cast<read_reg_node *>(n.rhs().owner())->regoff()) });

			assembler_.Bind(&end);
			break;
		case 32:
			assembler_.Bind(&retry);

			assembler_.lrw(out_reg, Address { dstAddr }, std::memory_order_seq_cst);
			assembler_.bne(out_reg, acc, &fail, Assembler::kNearJump);
			assembler_.scw(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);
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

			//Write back updated acc value
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

std::variant<Register, std::unique_ptr<Label>,std::monostate> riscv64_translation_context::materialise_binary_atomic(const binary_atomic_node &n)
{
	Register dstAddr = std::get<Register>(materialise(n.lhs().owner()));
	Register src = std::get<Register>(materialise(n.rhs().owner()));
	Register out_reg = T0;
	// FIXME Correct memory ordering?
	switch (n.op()) {
	case binary_atomic_op::xadd:
		switch (n.rhs().type().width()) {
		case 64:
			assembler_.amoaddd(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);
			assembler_.add(SF, out_reg, src); // Actual sum for flag generation
			break;
		case 32:
			assembler_.amoaddw(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);
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

		switch (n.rhs().type().width()) {
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
		switch (n.rhs().type().width()) {
		case 64:
			assembler_.amoaddd(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);

			assembler_.add(SF, out_reg, src); // Actual sum for flag generation
			break;
		case 32:
			assembler_.amoaddw(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);

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
		return std::monostate{};
	case binary_atomic_op::sub:
		assembler_.neg(out_reg, src);
		switch (n.rhs().type().width()) {
		case 64:
			assembler_.amoaddd(out_reg, out_reg, Address { dstAddr }, std::memory_order_seq_cst);

			assembler_.sub(SF, out_reg, src); // Actual difference for flag generation
			break;
		case 32:
			assembler_.amoaddw(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);

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
		return std::monostate{};
	case binary_atomic_op::band:
		switch (n.rhs().type().width()) {
		case 64:
			assembler_.amoandd(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);

			assembler_.and_(SF, out_reg, src); // Actual and for flag generation
			break;
		case 32:
			assembler_.amoandw(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);

			assembler_.and_(SF, out_reg, src); // Actual and for flag generation
			assembler_.slli(SF, SF, 32); // Get rid of higher 32 bits
			break;
		default:
			throw std::runtime_error("unsupported lock and width");
		}

		assembler_.seqz(ZF, SF); // ZF
		assembler_.sltz(SF, SF); // SF
		return std::monostate{};
	case binary_atomic_op::bor:
		switch (n.rhs().type().width()) {
		case 64:
			assembler_.amoord(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);

			assembler_.or_(SF, out_reg, src); // Actual or for flag generation
			break;
		case 32:
			assembler_.amoorw(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);

			assembler_.or_(SF, out_reg, src); // Actual or for flag generation
			assembler_.slli(SF, SF, 32); // Get rid of higher 32 bits
			break;
		default:
			throw std::runtime_error("unsupported lock or width");
		}

		assembler_.seqz(ZF, SF); // ZF
		assembler_.sltz(SF, SF); // SF
		return std::monostate{};
	case binary_atomic_op::bxor:
		switch (n.rhs().type().width()) {
		case 64:
			assembler_.amoxord(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);

			assembler_.xor_(SF, out_reg, src); // Actual xor for flag generation
			break;
		case 32:
			assembler_.amoxorw(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);

			assembler_.xor_(SF, out_reg, src); // Actual xor for flag generation
			assembler_.slli(SF, SF, 32); // Get rid of higher 32 bits
			break;
		default:
			throw std::runtime_error("unsupported lock xor width");
		}

		assembler_.seqz(ZF, SF); // ZF
		assembler_.sltz(SF, SF); // SF
		return std::monostate{};
	case binary_atomic_op::xchg:
		switch (n.rhs().type().width()) {
		case 64:
			assembler_.amoswapd(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);
			break;
		case 32:
			assembler_.amoswapw(out_reg, src, Address { dstAddr }, std::memory_order_seq_cst);
			break;
		default:
			throw std::runtime_error("unsupported xchg width");
		}

		switch (n.rhs().type().width()) {
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
	Register out_reg = T6;
	switch (n.op()) {

	case cast_op::bitcast:
	case cast_op::sx:
		// No-op
		return src_reg;

	case cast_op::zx:
		if (n.source_value().type().width() == 8) {
			assembler_.andi(out_reg, src_reg, 0xff);
		} else {
			assembler_.slli(out_reg, src_reg, 64 - n.source_value().type().width());
			assembler_.srli(out_reg, src_reg, 64 - n.source_value().type().width());
		}
		break;
	case cast_op::trunc:
		// Sign extend to preserve convention
		if (n.source_value().type().width() == 32) {
			assembler_.sextw(out_reg, src_reg);
		} else {
			assembler_.slli(out_reg, src_reg, 64 - n.source_value().type().width());
			assembler_.srai(out_reg, src_reg, 64 - n.source_value().type().width());
		}
		break;
	default:
		throw std::runtime_error("unsupported cast op");
	}
	return out_reg;
}

Register riscv64_translation_context::materialise_bit_extract(const bit_extract_node &n)
{
	Register out_reg = T4;
	int from = n.from();
	int length = n.length();

	Register src = std::get<Register>(materialise(n.source_value().owner()));

	// TODO Handle upper 64 from 128 for split Register multiply case

	if (length + from < 64) {
		assembler_.slli(out_reg, src, 64 - (from + length));
	}
	assembler_.srai(out_reg, out_reg, 64 - length); // Use arithmetic shift to keep sign extension up

	return out_reg;
}

Register riscv64_translation_context::materialise_bit_insert(const bit_insert_node &n)
{
	Register out_reg = T5;
	Register temp_reg = T6;
	int to = n.to();
	int length = n.length();

	// TODO Handle inserts for div with 128bits

	Register src = std::get<Register>(materialise(n.source_value().owner()));
	Register bits = std::get<Register>(materialise(n.bits().owner()));

	int64_t mask = ~(((1ll << length) - 1) << to);

	if (to == 0 && IsITypeImm(mask)) {
		// Since to==0 no shift necessary and just masking both is enough
		// `~mask` also fits IType since `mask` has all but lower bits set
		assembler_.andi(temp_reg, bits, ~mask);
		assembler_.andi(out_reg, src, mask);
	} else {
		// Fixme might override input
		Register mask_reg = materialise_constant(~mask);
		assembler_.and_(out_reg, src, mask_reg);
		assembler_.slli(temp_reg, bits, 64 - length);
		assembler_.srli(temp_reg, temp_reg, 64 - (length + to));
	}

	assembler_.or_(out_reg, out_reg, temp_reg);

	return out_reg;
}

Register riscv64_translation_context::materialise_read_reg(const read_reg_node &n) {
    const port &value = n.val();
    if (is_flag(value)) { // Flags
        // TODO
    } else if (is_gpr(value)) { // GPR
        Register out_reg = std::get<Register>(materialise(value.owner()));
        assembler_.ld(out_reg, { FP, static_cast<intptr_t>(n.regoff()) });

        return out_reg;
    }

    throw std::runtime_error("Unsupported width on register read: " +
                             std::to_string(value.type().width()));
}

void riscv64_translation_context::materialise_write_reg(const write_reg_node &n) {
    const port &value = n.val();
    if (is_flag(value)) { // Flags
        // TODO
    } else if (is_gpr(value)) { // GPR
        Register reg = std::get<Register>(materialise(value.owner()));
        assembler_.sd(reg, { FP, static_cast<intptr_t>(n.regoff()) });
        return;
    }

    throw std::runtime_error("Unsupported width on register write: " +
                             std::to_string(value.type().width()));
}

Register riscv64_translation_context::materialise_read_mem(const read_mem_node &n) {
    Register out_reg  = std::get<Register>(materialise(n.val().owner()));
    Register addr_reg = std::get<Register>(materialise(n.address().owner()));

    Address addr { addr_reg };
    auto load_instr = load_instructions.at(n.val().type().width());
    (assembler_.*load_instr)(out_reg, addr);

    return out_reg;
}

void riscv64_translation_context::materialise_write_mem(const write_mem_node &n) {
    Register src_reg  = std::get<Register>(materialise(n.val().owner()));
    Register addr_reg = std::get<Register>(materialise(n.address().owner()));

    // TODO: handle different sizes
    Address addr { addr_reg };
    auto store_instr = store_instructions.at(n.val().type().width());
    (assembler_.*store_instr)(src_reg, addr);
}

Register riscv64_translation_context::materialise_read_pc(const read_pc_node &n) {
    Register out_reg  = std::get<Register>(materialise(n.val().owner()));

    auto load_instr = load_instructions.at(n.val().type().width());
    assembler_.addi(out_reg, A1, 0);

    return out_reg;
}

void riscv64_translation_context::materialise_write_pc(const write_pc_node &n) {
    Register src_reg  = std::get<Register>(materialise(n.val().owner()));

    auto store_instr = store_instructions.at(n.val().type().width());
    assembler_.addi(A1, src_reg, 0);
}

std::unique_ptr<Label>
riscv64_translation_context::materialise_label(const label_node &n) {
    auto label = std::make_unique<Label>();
    assembler_.Bind(label.get());
    return label;
}

void riscv64_translation_context::materialise_br(const br_node &n) {
    auto label = std::move(std::get<std::unique_ptr<Label>>(materialise(n.target())));
    assembler_.j(label.get());
}

void riscv64_translation_context::materialise_cond_br(const cond_br_node &n) {
    Register cond = std::get<Register>(materialise(n.cond().owner()));

    auto label = std::move(std::get<std::unique_ptr<Label>>(materialise(n.target())));
    assembler_.beq(cond, ZERO, label.get());
}

Register riscv64_translation_context::materialise_unary_arith(const unary_arith_node& n) {
		Register out_reg = T0;

		Register src_reg = std::get<Register>(materialise(n.lhs().owner()));
		if (n.op() == unary_arith_op::bnot)
			assembler_.not_(out_reg, src_reg);
            return out_reg;

        throw std::runtime_error("unsupported unary arithmetic operation");
}

Register riscv64_translation_context::materialise_ternary_arith(const ternary_arith_node &n)
{
	Register out_reg = T0;

	Register src_reg1 = std::get<Register>(materialise(n.lhs().owner()));
	Register src_reg2 = std::get<Register>(materialise(n.rhs().owner()));
	Register src_reg3 = std::get<Register>(materialise(n.top().owner()));
	switch (n.op()) {
		// TODO Immediate handling

	case ternary_arith_op::adc:
		// Temporary: Add carry in
		switch (n.val().type().width()) {
		case 64:
			assembler_.add(ZF, src_reg2, src_reg3);
			break;
		case 32:
			assembler_.addw(ZF, src_reg2, src_reg3);
			break;
		case 8:
		case 16:
			assembler_.add(ZF, src_reg2, src_reg3);
			assembler_.slli(ZF, ZF, 64 - n.val().type().width());
			assembler_.srai(ZF, ZF, 64 - n.val().type().width());
			break;
		}

		assembler_.slt(OF, ZF, src_reg2); // Temporary overflow
		assembler_.sltu(CF, ZF, src_reg2); // Temporary carry

		// Normal add
		switch (n.val().type().width()) {
		case 64:
			assembler_.add(out_reg, src_reg1, ZF);
			break;
		case 32:
			assembler_.addw(out_reg, src_reg1, ZF);
			break;
		case 8:
		case 16:
			assembler_.add(out_reg, src_reg1, ZF);
			assembler_.slli(out_reg, out_reg, 64 - n.val().type().width());
			assembler_.srai(out_reg, out_reg, 64 - n.val().type().width());
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
		switch (n.val().type().width()) {
		case 64:
			assembler_.add(ZF, src_reg2, src_reg3);
			break;
		case 32:
			assembler_.addw(ZF, src_reg2, src_reg3);
			break;
		case 8:
		case 16:
			assembler_.add(ZF, src_reg2, src_reg3);
			assembler_.slli(ZF, ZF, 64 - n.val().type().width());
			assembler_.srai(ZF, ZF, 64 - n.val().type().width());
			break;
		}

		assembler_.slt(OF, ZF, src_reg2); // Temporary overflow
		assembler_.sltu(CF, ZF, src_reg2); // Temporary carry

		switch (n.val().type().width()) {
		case 64:
			assembler_.sub(out_reg, src_reg1, ZF);
			break;
		case 32:
			assembler_.subw(out_reg, src_reg1, ZF);
			break;
		case 16:
			assembler_.sub(out_reg, src_reg1, ZF);
			assembler_.slli(out_reg, out_reg, 64 - n.val().type().width());
			assembler_.srai(out_reg, out_reg, 64 - n.val().type().width());
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
	Register out_reg = T4;

	Register src_reg = std::get<Register>(materialise(n.input().owner()));

	if (n.amount().kind() == port_kinds::constant) {
		auto amt = (intptr_t)((constant_node *)n.amount().owner())->const_val_i() & 0x3f;
		if (amt == 0) {
			return src_reg;
		}
		switch (n.op()) {
		case shift_op::lsl:
			assembler_.srli(CF, src_reg, n.val().type().width() - amt); // CF

			if (amt == 1) {
				assembler_.srli(OF, src_reg, n.val().type().width() - amt - 1); // OF
				assembler_.xor_(OF, OF, CF);
				assembler_.andi(OF, OF, 1);
			}

			switch (n.val().type().width()) {
			case 64:
				assembler_.slli(out_reg, src_reg, amt);
				break;
			case 32:
				assembler_.slliw(out_reg, src_reg, amt);
				break;
			case 8:
			case 16:
				assembler_.slli(out_reg, src_reg, amt + (64 - n.val().type().width()));
				assembler_.srai(out_reg, src_reg, (64 - n.val().type().width()));
				break;
			}

			break;
		case shift_op::lsr:
			assembler_.srli(CF, src_reg, amt - 1); // CF
			switch (n.val().type().width()) {
			case 64:
				assembler_.srli(out_reg, src_reg, amt);
				break;
			case 32:
				assembler_.srliw(out_reg, src_reg, amt);
				break;
			case 16:
				assembler_.slli(out_reg, src_reg, 48);
				assembler_.srli(out_reg, src_reg, 48 + amt);
				break;
			case 8:
				assembler_.andi(out_reg, src_reg, 0xff);
				assembler_.srli(out_reg, src_reg, amt);
				break;
			}
			if (amt == 1) {
				assembler_.srli(OF, src_reg, n.val().type().width() - 1);
				assembler_.andi(OF, OF, 1); // OF
			}
			break;
		case shift_op::asr:
			assembler_.srli(CF, src_reg, amt - 1); // CF
			assembler_.srai(out_reg, src_reg, amt); // Sign extension preserved
			if (amt == 1) {
				assembler_.li(OF, 0);
			}
			break;
		}

		assembler_.andi(CF, CF, 1); // CF

		assembler_.seqz(ZF, out_reg); // ZF
		assembler_.sltz(SF, out_reg); // SF

		return out_reg;
	}

	auto amount = std::get<Register>(materialise(n.amount().owner()));
	assembler_.subi(OF, amount, 1);

	switch (n.op()) {
	case shift_op::lsl:
		assembler_.sll(CF, src_reg, OF); // CF in highest bit

		switch (n.val().type().width()) {
		case 64:
			assembler_.sll(out_reg, src_reg, amount);
			break;
		case 32:
			assembler_.sllw(out_reg, src_reg, amount);
			break;
		case 8:
		case 16:
			assembler_.sll(out_reg, src_reg, amount);
			assembler_.slli(out_reg, src_reg, (64 - n.val().type().width()));
			assembler_.srai(out_reg, src_reg, (64 - n.val().type().width()));
			break;
		}

		assembler_.xor_(OF, out_reg, CF); // OF in highest bit

		assembler_.srli(CF, CF, n.val().type().width() - 1);

		assembler_.srli(OF, OF, n.val().type().width() - 1);
		assembler_.andi(OF, OF, 1); // OF
		break;
	case shift_op::lsr:
		assembler_.srl(CF, src_reg, OF); // CF
		switch (n.val().type().width()) {
		case 64:
			assembler_.srl(out_reg, src_reg, amount);
			break;
		case 32:
			assembler_.srlw(out_reg, src_reg, amount);
			break;
		case 16:
			assembler_.slli(out_reg, src_reg, 48);
			assembler_.srli(out_reg, src_reg, 48);
			assembler_.srl(out_reg, src_reg, amount);
			break;
		case 8:
			assembler_.andi(out_reg, src_reg, 0xff);
			assembler_.srl(out_reg, src_reg, amount);
			break;
		}

		// Undefined on shift amt > 1 (use same formula as amt == 1)
		assembler_.srli(OF, src_reg, n.val().type().width() - 1);
		assembler_.andi(OF, OF, 1); // OF

		break;
	case shift_op::asr:
		assembler_.srl(CF, src_reg, OF); // CF
		assembler_.li(OF, 0); // OF

		assembler_.sra(out_reg, src_reg, amount); // Sign extension preserved

		break;
	}

	assembler_.andi(CF, CF, 1); // Limit CF to single bit
	assembler_.seqz(ZF, out_reg); // ZF
	assembler_.sltz(SF, out_reg); // SF

	return out_reg;
}

Register riscv64_translation_context::materialise_binary_arith(const binary_arith_node &n)
{
	Register out_reg = T0;
	Register out_reg2 = T1;

	Register src_reg1 = std::get<Register>(materialise(n.lhs().owner()));

	if (n.rhs().owner()->kind() == node_kinds::constant) {
		// Could also work for LHS except sub
		// TODO Probably incorrect to just cast to signed 64bit
		auto imm = (intptr_t)((constant_node *)(n.rhs().owner()))->const_val_i();
		if (imm)

			if (IsITypeImm(imm)) {
				switch (n.op()) {

				case binary_arith_op::sub:
					if (imm == -2048) { // Can happen with inversion
						goto standardPath;
					}
					switch (n.val().type().width()) {
					case 64:
						assembler_.addi(out_reg, src_reg1, -imm);
						break;
					case 32:
						assembler_.addiw(out_reg, src_reg1, -imm);
						break;
					case 8:
					case 16:
						assembler_.addi(out_reg, src_reg1, -imm);
						assembler_.slli(out_reg, out_reg, 64 - n.val().type().width());
						assembler_.srai(out_reg, out_reg, 64 - n.val().type().width());
						break;
					}
					assembler_.sltu(CF, src_reg1, out_reg); // CF FIXME Assumes out_reg!=src_reg1
					assembler_.slt(OF, out_reg, src_reg1); // OF FIXME Assumes out_reg!=src_reg1
					if (imm > 0) {
						assembler_.xori(OF, OF, 1); // Invert on positive
					}
					break;

				case binary_arith_op::add:

					switch (n.val().type().width()) {
					case 64:
						assembler_.addi(out_reg, src_reg1, imm);
						break;
					case 32:
						assembler_.addiw(out_reg, src_reg1, -imm);
						break;
					case 8:
					case 16:
						assembler_.addi(out_reg, src_reg1, imm);
						assembler_.slli(out_reg, out_reg, 64 - n.val().type().width());
						assembler_.srai(out_reg, out_reg, 64 - n.val().type().width());
						break;
					}

					assembler_.sltu(CF, out_reg, src_reg1); // CF FIXME Assumes out_reg!=src_reg1
					assembler_.slt(OF, out_reg, src_reg1); // OF FIXME Assumes out_reg!=src_reg1
					if (imm < 0) {
						assembler_.xori(OF, OF, 1); // Invert on negative
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

				assembler_.seqz(ZF, out_reg); // ZF
				assembler_.sltz(SF, out_reg); // SF

				return out_reg;
			}
	}

standardPath:
	Register src_reg2 = std::get<Register>(materialise(n.rhs().owner()));
	switch (n.op()) {

	case binary_arith_op::add:
		switch (n.val().type().width()) {
		case 64:
			assembler_.add(out_reg, src_reg1, src_reg2);
			break;
		case 32:
			assembler_.addw(out_reg, src_reg1, src_reg2);
			break;
		case 8:
		case 16:
			assembler_.add(out_reg, src_reg1, src_reg2);
			assembler_.slli(out_reg, out_reg, 64 - n.val().type().width());
			assembler_.srai(out_reg, out_reg, 64 - n.val().type().width());
			break;
		}

		assembler_.sltz(CF, src_reg1);
		assembler_.slt(OF, out_reg, src_reg2);
		assembler_.xor_(OF, OF, CF); // OF FIXME Assumes out_reg!=src_reg1 && out_reg!=src_reg2

		assembler_.sltu(CF, out_reg, src_reg2); // CF (Allows typical x86 case of regSrc1==out_reg) FIXME Assumes out_reg!=src_reg2
		break;

	case binary_arith_op::sub:
		switch (n.val().type().width()) {
		case 64:
			assembler_.sub(out_reg, src_reg1, src_reg2);
			break;
		case 32:
			assembler_.subw(out_reg, src_reg1, src_reg2);
			break;
		case 8:
		case 16:
			assembler_.sub(out_reg, src_reg1, src_reg2);
			assembler_.slli(out_reg, out_reg, 64 - n.val().type().width());
			assembler_.srai(out_reg, out_reg, 64 - n.val().type().width());
			break;
		}

		assembler_.sgtz(CF, src_reg2);
		assembler_.slt(OF, out_reg, src_reg1);
		assembler_.xor_(OF, OF, CF); // OF FIXME Assumes out_reg!=src_reg1 && out_reg!=src_reg2

		assembler_.sltu(CF, src_reg1, out_reg); // CF FIXME Assumes out_reg!=src_reg1
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
		break;

	case binary_arith_op::mul:
		switch (n.val().type().width()) {
		case 128:
			// Split calculation
			assembler_.mul(out_reg, src_reg1, src_reg2);
			switch (n.val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				assembler_.mulh(out_reg2, src_reg1, src_reg2);
				assembler_.srai(CF, out_reg, 64);
				assembler_.xor_(CF, CF, out_reg2);
				assembler_.snez(CF, CF);
				break;
			case value_type_class::unsigned_integer:
				assembler_.mulhu(out_reg2, src_reg1, src_reg2);
				assembler_.snez(CF, out_reg2);
				break;
			default:
				throw std::runtime_error("Unsupported value type for multiply");
			}
			assembler_.mv(OF, CF);
			break;

		case 64:
		case 32:
		case 16:
			assembler_.mul(out_reg, src_reg1, src_reg2); // Assumes proper signed/unsigned extension from 32/16/8 bits

			switch (n.val().type().element_type().type_class()) {

			case value_type_class::signed_integer:
				if (n.val().type().width() == 64) {
					assembler_.sextw(CF, out_reg);
				} else {
					assembler_.slli(CF, out_reg, 64 - (n.val().type().width()) / 2);
					assembler_.srai(CF, out_reg, 64 - (n.val().type().width()) / 2);
				}
				assembler_.xor_(CF, CF, out_reg);
				assembler_.snez(CF, CF);
				break;
			case value_type_class::unsigned_integer:
				assembler_.srli(CF, out_reg, n.val().type().width() / 2);
				assembler_.snez(CF, out_reg2);
				break;
			default:
				throw std::runtime_error("Unsupported value type for multiply");
			}
			assembler_.mv(OF, CF);
			break;
		}
		break;
	case binary_arith_op::div:
		switch (n.val().type().width()) {
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
		}
		break;
	case binary_arith_op::mod:
		switch (n.val().type().width()) {
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
		}
		break;

	case binary_arith_op::cmpeq:
		assembler_.xor_(out_reg, src_reg1, src_reg2);
		assembler_.seqz(out_reg, out_reg);
		break;
	case binary_arith_op::cmpne:
		assembler_.xor_(out_reg, src_reg1, src_reg2);
		assembler_.snez(out_reg, out_reg);
		break;
	case binary_arith_op::cmpgt:
		throw std::runtime_error("unsupported binary arithmetic operation");
	}

	// TODO those should only be set on add, sub, xor, or, and
	assembler_.seqz(ZF, out_reg); // ZF
	assembler_.sltz(SF, out_reg); // SF

	return out_reg;
}
Register riscv64_translation_context::materialise_constant(int64_t imm)
{
	// Optimizations with left or right shift at the end not implemented (for constants with trailing or leading zeroes)

	if (imm == 0) {
		return ZERO;
	}
	Register out_reg = A0;
	auto immLo32 = (int32_t)imm;
	auto immHi32 = imm >> 32 << 32;
	auto immLo12 = immLo32 << (32 - 12) >> (32 - 12); // sign extend lower 12 bit
	if (immHi32 == 0) {
		int32_t imm32Hi20 = (immLo32 - immLo12);
		if (imm32Hi20 != 0) {
			assembler_.lui(out_reg, imm32Hi20);
			if (immLo12) {
				assembler_.addiw(out_reg, out_reg, immLo12);
			}
		} else {
			assembler_.li(out_reg, imm);
		}

	} else {
		auto val = (int64_t)((uint64_t)imm - (uint64_t)(int64_t)immLo12); // Get lower 12 bits out of imm
		int shiftAmnt = 0;
		if (!Utils::IsInt(32, val)) { // Might still not fit as LUI with unsigned add
			shiftAmnt = __builtin_ctzll(val);
			val >>= shiftAmnt;
			if (shiftAmnt > 12 && !IsITypeImm(val)
				&& Utils::IsInt(32, val << 12)) { // Does not fit into 12 bits but can fit into LUI U-immediate with proper shift
				val <<= 12;
				shiftAmnt -= 12;
			}
		}

		materialise_constant(val);

		if (shiftAmnt) {
			assembler_.slli(out_reg, out_reg, shiftAmnt);
		}

		if (immLo12) {
			assembler_.addi(out_reg, out_reg, immLo12);
		}
	}
	return out_reg;
}

