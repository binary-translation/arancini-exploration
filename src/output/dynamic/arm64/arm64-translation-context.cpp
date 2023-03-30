#include <arancini/output/dynamic/arm64/arm64-translation-context.h>

#include <arancini/runtime/exec/x86/x86-cpu-state.h>

#include <cctype>
#include <unordered_map>

using namespace arancini::output::dynamic::arm64;
using namespace arancini::ir;

arm64_physreg_op memory_base_reg(arm64_physreg_op::x18);
arm64_physreg_op context_block_reg(arm64_physreg_op::x29);

#define X86_OFFSET_OF(reg) __builtin_offsetof(struct arancini::runtime::exec::x86::x86_cpu_state, reg)
enum class reg_offsets {
#define DEFREG(ctype, ltype, name) name = X86_OFFSET_OF(name),
#include <arancini/input/x86/reg.def>
#undef DEFREG
};

const arm64_physreg_op ZF(arm64_physreg_op::x10);
const arm64_physreg_op CF(arm64_physreg_op::x11);
const arm64_physreg_op OF(arm64_physreg_op::x12);
const arm64_physreg_op SF(arm64_physreg_op::x13);

static std::unordered_map<unsigned long, int> flag_map {
	{ (unsigned long)reg_offsets::ZF, {} },
	{ (unsigned long)reg_offsets::CF, {} },
	{ (unsigned long)reg_offsets::OF, {} },
	{ (unsigned long)reg_offsets::SF, {} },
};

static bool is_gpr(const port &p) {
    auto w = p.type().element_width();
    return w == 8 || w == 16 || w == 32 || w == 64;
}

static bool is_flag(const port &p) {
    return p.type().element_width() == 1;
}

static inline bool is_flag_port(const port &value) {
	return value.kind() == port_kinds::zero ||
           value.kind() == port_kinds::carry ||
           value.kind() == port_kinds::negative ||
           value.kind() == port_kinds::overflow;
}

static arm64_operand virtreg_operand(unsigned int index, int width) {
    return arm64_operand(arm64_vreg_op(index, width == 1 ? 8 : width));
}

static arm64_operand imm_operand(unsigned long value, int width) {
    return arm64_operand(arm64_immediate_operand(value, width == 1 ? 8 : width));
}

arm64_operand
arm64_translation_context::guestreg_memory_operand(int width, int regoff,
                                                   bool pre, bool post)
{
    // FIXME: handle width
    arm64_memory_operand mem;
    if (regoff > 255 || regoff < -256) {
        auto base_vreg = alloc_vreg();
        auto preg = context_block_reg;
        builder_.mov(virtreg_operand(base_vreg, width),
                     imm_operand(regoff, 16));
        builder_.add(virtreg_operand(base_vreg, width),
                     arm64_operand(preg),
                     virtreg_operand(base_vreg, width));
        mem = arm64_memory_operand(arm64_vreg_op(base_vreg, width), 0, pre, post);
    } else {
        auto preg = context_block_reg;
        mem = arm64_memory_operand(preg, regoff, pre, post);
    }

	return arm64_operand(mem);
}

arm64_operand arm64_translation_context::vreg_operand_for_port(port &p, bool constant_fold) {
    // TODO
	if (constant_fold) {
		if (p.owner()->kind() == node_kinds::read_pc) {
			return arm64_operand(arm64_immediate_operand(this_pc_, 64));
		} else if (p.owner()->kind() == node_kinds::constant) {
			return arm64_operand(arm64_immediate_operand(((constant_node *)p.owner())->const_val_i(), p.type().width()));
		}
	}

	materialise(p.owner());
	return virtreg_operand(vreg_for_port(p), p.type().element_width());
}

static void func_push_args(arm64_instruction_builder *builder, const
                           std::vector<port *> &args)
{
    if (!builder)
        throw std::runtime_error("Instruction builder is invalid");

    for (auto* arg : args) {
        // TODO: handle the calling convention
        // this depends on the expectations of the function
        // TODO: maybe mark all functions as asm_linkage for stack-based
        // argument passing?
    }
}

void arm64_translation_context::begin_block() {
    ret_ = 0;
    instr_cnt_ = 0;
    builder_ = arm64_instruction_builder();
}

std::string labelify(const std::string &str) {
    std::string label;
    for (auto c : str) {
        if (std::ispunct(c))
            c = '_';
        if (std::isspace(c))
            c = '_';
        label.push_back(c);
    }

    return label;
}

void arm64_translation_context::begin_instruction(off_t address, const std::string &disasm) {
	instruction_index_to_guest_[builder_.nr_instructions()] = address;

	this_pc_ = address;
	std::cerr << "  " << std::hex << address << ": " << disasm << std::endl;

    instr_cnt_++;

    builder_.insert_sep("S" + std::to_string(instr_cnt_) + labelify(disasm));
}

void arm64_translation_context::end_instruction() { }

void arm64_translation_context::end_block() {
	do_register_allocation();

    // Return value in x0 = 0;
	builder_.mov(arm64_operand(arm64_physreg_op(arm64_physreg_op::x0)),
                 imm_operand(ret_, 64));
	builder_.ret();

	// builder_.dump(std::cerr);

	builder_.emit(writer());
}

void arm64_translation_context::lower(ir::node *n) {
    materialise(n);
}

void arm64_translation_context::materialise(const ir::node* n) {
    if (!n)
        throw std::runtime_error("ARM64 DBT received NULL pointer to node");

    switch (n->kind()) {
    case node_kinds::read_reg:
        return materialise_read_reg(*reinterpret_cast<const read_reg_node*>(n));
    case node_kinds::write_reg:
        return materialise_write_reg(*reinterpret_cast<const write_reg_node*>(n));
    case node_kinds::read_mem:
        return materialise_read_mem(*reinterpret_cast<const read_mem_node*>(n));
    case node_kinds::write_mem:
        return materialise_write_mem(*reinterpret_cast<const write_mem_node*>(n));
	case node_kinds::read_pc:
		return materialise_read_pc(*reinterpret_cast<const read_pc_node *>(n));
	case node_kinds::write_pc:
		return materialise_write_pc(*reinterpret_cast<const write_pc_node *>(n));
    case node_kinds::label:
        return materialise_label(*reinterpret_cast<const label_node *>(n));
    case node_kinds::br:
        return materialise_br(*reinterpret_cast<const br_node *>(n));
    case node_kinds::cond_br:
        return materialise_cond_br(*reinterpret_cast<const cond_br_node *>(n));
	case node_kinds::cast:
		return materialise_cast(*reinterpret_cast<const cast_node *>(n));
    case node_kinds::csel:
		return materialise_csel(*reinterpret_cast<const csel_node *>(n));
    case node_kinds::bit_shift:
		return materialise_bit_shift(*reinterpret_cast<const bit_shift_node *>(n));
    case node_kinds::bit_extract:
		return materialise_bit_extract(*reinterpret_cast<const bit_extract_node *>(n));
    case node_kinds::bit_insert:
		return materialise_bit_insert(*reinterpret_cast<const bit_insert_node *>(n));
    case node_kinds::constant:
        return materialise_constant(*reinterpret_cast<const constant_node*>(n));
	case node_kinds::unary_arith:
        return materialise_unary_arith(*reinterpret_cast<const unary_arith_node*>(n));
	case node_kinds::binary_arith:
		return materialise_binary_arith(*reinterpret_cast<const binary_arith_node*>(n));
    case node_kinds::internal_call:
        return materialise_internal_call(*reinterpret_cast<const internal_call_node*>(n));
    default:
        throw std::runtime_error("unknown node encountered: " +
                                 std::to_string(static_cast<size_t>(n->kind())));
    }
}

void arm64_translation_context::materialise_read_reg(const read_reg_node &n) {
    // TODO: deal with widths
	int dest_vreg = alloc_vreg_for_port(n.val());
	int w = n.val().type().element_width() == 1 ? 8 : n.val().type().element_width();

	builder_.ldr(virtreg_operand(dest_vreg, w),
                 guestreg_memory_operand(w, n.regoff()));
}

void arm64_translation_context::materialise_write_reg(const write_reg_node &n) {
	int w = n.value().type().element_width();

    if (!is_gpr(n.value()) && !is_flag(n.value()))
        throw std::runtime_error("Invalid destination width on register write: "
                                 + std::to_string(w));

    // TODO: deal with widths
    arm64_operand reg;
    if (is_flag(n.value()) && is_flag_port(n.value())) {
        reg = virtreg_operand(flag_map.at(n.regoff()), 64);
    } else {
        reg = vreg_operand_for_port(n.value());
    }

    builder_.str(reg, guestreg_memory_operand(w, n.regoff()));
}

void arm64_translation_context::materialise_read_mem(const read_mem_node &n) {
	int dest_vreg = alloc_vreg_for_port(n.val());
	int w = n.val().type().element_width();

    // Add base to addr_vreg
    int addr = alloc_vreg();
    builder_.add(virtreg_operand(addr, 64),
                 memory_base_reg,
                 vreg_operand_for_port(n.address()));

    // TODO: widths
    auto mem = arm64_memory_operand(arm64_vreg_op(addr, w));
	builder_.ldr(virtreg_operand(dest_vreg, w),
                 arm64_operand(mem));
}

void arm64_translation_context::materialise_write_mem(const write_mem_node &n) {
	int w = n.value().type().element_width();

	auto addr_vreg = vreg_operand_for_port(n.address());

    // TODO: separate to function
    int addr = alloc_vreg();
    builder_.add(virtreg_operand(addr, 64),
                 memory_base_reg,
                 addr_vreg);

    auto mem = arm64_memory_operand(arm64_vreg_op(addr, w), 0, false, true);
	builder_.str(vreg_operand_for_port(n.value()),
                 arm64_operand(mem));
}

void arm64_translation_context::materialise_read_pc(const read_pc_node &n) {
	int dst_vreg = alloc_vreg_for_port(n.val());
    int w = n.val().type().element_width();

    // TODO: check out immediate size
	builder_.ldr(virtreg_operand(dst_vreg, w),
                 guestreg_memory_operand(w, static_cast<int>(reg_offsets::PC)));
}

void arm64_translation_context::materialise_write_pc(const write_pc_node &n) {
    auto value_vreg = vreg_operand_for_port(n.value());
    size_t w = value_vreg.width();

    builder_.str(value_vreg,
                 guestreg_memory_operand(w, static_cast<int>(reg_offsets::PC)));
}

void arm64_translation_context::materialise_label(const label_node &n) {
    if (!builder_.has_label(n.name() + ":"))
        builder_.label(n.name());
}

void arm64_translation_context::materialise_br(const br_node &n) {
    materialise(n.target());

    builder_.b(n.target()->name());
}

void arm64_translation_context::materialise_cond_br(const cond_br_node &n) {
    materialise(n.target());

    // TODO: width
    builder_.cmp(vreg_operand_for_port(n.cond()),
                 imm_operand(0, 64));
    builder_.beq(n.target()->name());
}

void arm64_translation_context::materialise_constant(const constant_node &n) {
    // TODO: width
    // TODO: handle immediate size that cannot be stored
	int dst_vreg = alloc_vreg_for_port(n.val());

    int w = n.val().type().element_width();

    // TODO: check width
    // TODO: what if floating point?
	builder_.mov(virtreg_operand(dst_vreg, w),
                 imm_operand(n.const_val_i(), w));
}

void arm64_translation_context::materialise_binary_arith(const binary_arith_node &n) {
	int val_vreg = alloc_vreg_for_port(n.val());

    flag_map[(unsigned long)reg_offsets::ZF] = alloc_vreg_for_port(n.zero());
    flag_map[(unsigned long)reg_offsets::SF] = alloc_vreg_for_port(n.negative());
    flag_map[(unsigned long)reg_offsets::OF] = alloc_vreg_for_port(n.overflow());
    flag_map[(unsigned long)reg_offsets::CF] = alloc_vreg_for_port(n.carry());

	int w = n.val().type().element_width();

    // TODO: check
    const char* mod = (w == 16 ? "UXTX" : "UXTX");
	switch (n.op()) {
	case binary_arith_op::add:
        if (w == 8 || w == 16) {
            builder_.add(virtreg_operand(val_vreg, w),
                         vreg_operand_for_port(n.lhs()),
                         vreg_operand_for_port(n.rhs()),
                         arm64_shift_operand(mod, 0, 64));
        } else {
            builder_.add(virtreg_operand(val_vreg, w),
                         vreg_operand_for_port(n.lhs()),
                         vreg_operand_for_port(n.rhs()));
        }
		break;
	case binary_arith_op::sub:
        if (w == 8 || w == 16) {
            builder_.add(virtreg_operand(val_vreg, w),
                         vreg_operand_for_port(n.lhs()),
                         vreg_operand_for_port(n.rhs()),
                         arm64_shift_operand(mod, 0, 64));
        } else {
            builder_.add(virtreg_operand(val_vreg, w),
                         vreg_operand_for_port(n.lhs()),
                         vreg_operand_for_port(n.rhs()));
        }
		break;
	case binary_arith_op::mul:
		builder_.mul(virtreg_operand(val_vreg, w),
                     vreg_operand_for_port(n.lhs()),
                     vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::div:
		builder_.sdiv(virtreg_operand(val_vreg, w),
                      vreg_operand_for_port(n.lhs()),
                      vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::mod:
		builder_.and_(virtreg_operand(val_vreg, w),
                      vreg_operand_for_port(n.lhs()),
                      vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::bor:
		builder_.or_(virtreg_operand(val_vreg, w),
                     vreg_operand_for_port(n.lhs()),
                     vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::band:
		builder_.and_(virtreg_operand(val_vreg, w),
                      vreg_operand_for_port(n.lhs()),
                      vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::bxor:
		builder_.xor_(virtreg_operand(val_vreg, w),
                      vreg_operand_for_port(n.lhs()),
                      vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::cmpeq:
        // NOTE: set*() will modify the flags
        builder_.cmp(vreg_operand_for_port(n.lhs()), vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::cmpne:
        builder_.cmp(vreg_operand_for_port(n.lhs()), vreg_operand_for_port(n.rhs()));
		break;
	case binary_arith_op::cmpgt:
        builder_.cmp(vreg_operand_for_port(n.lhs()), vreg_operand_for_port(n.rhs()));
		break;
	default:
		throw std::runtime_error("unsupported binary arithmetic operation " + std::to_string((int)n.op()));
	}

    // FIXME Another write-reg node generated?
	builder_.setz(virtreg_operand(flag_map[(unsigned long)reg_offsets::ZF], 64));
	builder_.sets(virtreg_operand(flag_map[(unsigned long)reg_offsets::SF], 64));
	builder_.seto(virtreg_operand(flag_map[(unsigned long)reg_offsets::OF], 64));
	builder_.setc(virtreg_operand(flag_map[(unsigned long)reg_offsets::CF], 64));
}

void arm64_translation_context::materialise_unary_arith(const unary_arith_node &n) {
	int val_vreg = alloc_vreg_for_port(n.val());

    int w = n.val().type().element_width();

    switch (n.op()) {
    case unary_arith_op::bnot:
        builder_.not_(virtreg_operand(val_vreg, w), vreg_operand_for_port(n.lhs()));
        break;
    case unary_arith_op::neg:
        // neg: ~reg + 1 for complement-of-2
        builder_.not_(virtreg_operand(val_vreg, w), vreg_operand_for_port(n.lhs()));
        builder_.add(virtreg_operand(val_vreg, w),
                     virtreg_operand(val_vreg, w),
                     imm_operand(1, 64));
        break;
    default:
        throw std::runtime_error("Unknown unary operation");
    }
}

void arm64_translation_context::materialise_cast(const cast_node &n) {
	int dst_vreg = alloc_vreg_for_port(n.val());

	switch (n.op()) {
	case cast_op::zx:
		builder_.movz(virtreg_operand(dst_vreg, n.val().type().element_width()),
                      vreg_operand_for_port(n.source_value()),
                      arm64_shift_operand("LSL", 0, 64));
		break;

	case cast_op::sx:
		builder_.movn(virtreg_operand(dst_vreg, n.val().type().element_width()),
                      vreg_operand_for_port(n.source_value(), false),
                      arm64_shift_operand("LSL", 0, 64));
		break;

	case cast_op::bitcast:
		builder_.mov(virtreg_operand(dst_vreg, n.val().type().element_width()),
                     vreg_operand_for_port(n.source_value()));
		break;

	default:
		throw std::runtime_error("unsupported cast operation");
	}
}

void arm64_translation_context::materialise_csel(const csel_node &n) {
    int dst_vreg = alloc_vreg_for_port(n.val());

    builder_.csel(virtreg_operand(dst_vreg, n.val().type().element_width()),
                  vreg_operand_for_port(n.trueval()),
                  vreg_operand_for_port(n.falseval()),
                  vreg_operand_for_port(n.condition()));
}

void arm64_translation_context::materialise_bit_shift(const bit_shift_node &n) {
    int dst_vreg = alloc_vreg_for_port(n.val());

    switch (n.op()) {
    case shift_op::lsl:
        builder_.lsl(virtreg_operand(dst_vreg, n.val().type().element_width()),
                     vreg_operand_for_port(n.input()),
                     vreg_operand_for_port(n.amount()));
        break;
    case shift_op::lsr:
        builder_.lsr(virtreg_operand(dst_vreg, n.val().type().element_width()),
                     vreg_operand_for_port(n.input()),
                     vreg_operand_for_port(n.amount()));
        break;
    case shift_op::asr:
        builder_.asr(virtreg_operand(dst_vreg, n.val().type().element_width()),
                     vreg_operand_for_port(n.input()),
                     vreg_operand_for_port(n.amount()));
        break;
    default:
        throw std::runtime_error("unsupported shift operation: " +
                                 std::to_string(static_cast<int>(n.op())));
    }
}

void arm64_translation_context::materialise_bit_extract(const bit_extract_node &n) {
    int dst_vreg = alloc_vreg_for_port(n.val());

    builder_.ubfx(virtreg_operand(dst_vreg, n.val().type().element_width()),
                  vreg_operand_for_port(n.source_value(), false),
                  imm_operand(n.from(), 64),
                  imm_operand(n.length(), 64));
}

void arm64_translation_context::materialise_bit_insert(const bit_insert_node &n) {
    int dst_vreg = alloc_vreg_for_port(n.val());

    // TODO
    builder_.bfi(virtreg_operand(dst_vreg, n.val().type().element_width()),
                 vreg_operand_for_port(n.bits(), false),
                 imm_operand(n.to(), 64),
                 imm_operand(n.length(), 64));
}

void arm64_translation_context::materialise_internal_call(const internal_call_node &n) {
    // TODO
    // func_push_args(&builder_, n.args());
    // builder_.bl(n.fn().name());
    if (n.fn().name() == "handle_syscall") {
        int pc_vreg = alloc_vreg();
        auto pc_op = virtreg_operand(pc_vreg, 64);
        builder_.movz(pc_op,
                      imm_operand((this_pc_ + 2) & 0xFFFF, 16),
                      arm64_shift_operand("LSL", 0));
        builder_.movk(pc_op,
                      imm_operand((this_pc_ + 2) >> 16 & 0xFFFF, 16),
                      arm64_shift_operand("LSL", 16));
        builder_.movk(pc_op,
                      imm_operand((this_pc_ + 2) >> 32 & 0xFFFF, 16),
                      arm64_shift_operand("LSL", 32));
        builder_.movk(pc_op,
                      imm_operand((this_pc_ + 2) >> 48 & 0xFFFF, 16),
                      arm64_shift_operand("LSL", 48));
        builder_.str(pc_op,
                     guestreg_memory_operand(64, static_cast<int>(reg_offsets::PC)));
        ret_ = 1;
    } else if (n.fn().name() == "handle_int") {
        ret_ = 2;
    } else {
        throw std::runtime_error("unsupported internal call");
    }
}

void arm64_translation_context::do_register_allocation() { builder_.allocate(); }

