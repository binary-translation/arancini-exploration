#include "arancini/ir/node.h"
#include "arancini/ir/port.h"
#include "arancini/ir/value-type.h"
#include "arancini/output/dynamic/arm64/arm64-instruction.h"
#include <arancini/output/dynamic/arm64/arm64-translation-context.h>

#include <arancini/runtime/exec/x86/x86-cpu-state.h>

#include <cmath>
#include <cctype>
#include <cstddef>
#include <string>
#include <stdexcept>
#include <unordered_map>

using namespace arancini::output::dynamic::arm64;
using namespace arancini::ir;

preg_operand memory_base_reg(preg_operand::x18);
preg_operand context_block_reg(preg_operand::x29);

#define X86_OFFSET_OF(reg) __builtin_offsetof(struct arancini::runtime::exec::x86::x86_cpu_state, reg)
enum class reg_offsets {
#define DEFREG(ctype, ltype, name) name = X86_OFFSET_OF(name),
#include <arancini/input/x86/reg.def>
#undef DEFREG
};

const preg_operand ZF(preg_operand::x10);
const preg_operand CF(preg_operand::x11);
const preg_operand OF(preg_operand::x12);
const preg_operand SF(preg_operand::x13);

static std::unordered_map<unsigned long, vreg_operand> flag_map {
	{ (unsigned long)reg_offsets::ZF, {} },
	{ (unsigned long)reg_offsets::CF, {} },
	{ (unsigned long)reg_offsets::OF, {} },
	{ (unsigned long)reg_offsets::SF, {} },
};

value_type addr_type() {
    return value_type::u64();
}

value_type base_type() {
    return value_type::u64();
}

memory_operand
arm64_translation_context::guestreg_memory_operand(int regoff, bool pre, bool post)
{
    memory_operand mem;
    if (regoff > 255 || regoff < -256) {
        auto preg = context_block_reg;
        auto base_vreg = vreg_operand(alloc_vreg(), addr_type());
        builder_.mov(base_vreg, immediate_operand(regoff, value_type::u32()));
        builder_.add(base_vreg, preg, base_vreg);
        mem = memory_operand(base_vreg, 0, pre, post);
    } else {
        auto preg = context_block_reg;
        mem = memory_operand(preg, regoff, pre, post);
    }

	return mem;
}

std::vector<vreg_operand> arm64_translation_context::vreg_operand_for_port(port &p, bool constant_fold) {
    // TODO
	if (constant_fold) {
		if (p.owner()->kind() == node_kinds::read_pc) {
            // PC always represented by 64-bit value
			return {mov_immediate(this_pc_, value_type::u64())};
		} else if (p.owner()->kind() == node_kinds::constant) {
			return {mov_immediate(((constant_node *)p.owner())->const_val_i(), p.type())};
		}
	}

	materialise(p.owner());
	return vregs_for_port(p);
}

vreg_operand arm64_translation_context::add_membase(const vreg_operand &addr) {
    auto mem_addr_vreg = vreg_operand(alloc_vreg(), addr_type());
    builder_.add(mem_addr_vreg, memory_base_reg, addr);

    return mem_addr_vreg;
}

vreg_operand arm64_translation_context::mov_immediate(uint64_t imm, value_type type) {
    size_t actual_size = static_cast<size_t>(std::ceil(std::log2(imm)));
    size_t move_count = static_cast<size_t>(std::ceil(actual_size / 16.0));

    // TODO: it seems like the frontend generates very large constants
    // represented as s32(). This fails here, since the resultiing value does not
    // fit
    if (actual_size <= 16) {
        auto reg = vreg_operand(alloc_vreg(), type);
        builder_.mov(reg, immediate_operand(imm, value_type::u16()));
        return reg;
    }

    auto reg = vreg_operand(alloc_vreg(), value_type::u64());
    if (actual_size <= 64) {
        builder_.movz(reg,
                      immediate_operand(imm & 0xFFFF, value_type::u16()),
                      shift_operand("LSL", 0));
        for (size_t i = 1; i < move_count; ++i) {
            builder_.movk(reg,
                          immediate_operand(imm >> (i * 16) & 0xFFFF, value_type::u16()),
                          shift_operand("LSL", (i * 16), value_type::u64()));
        }

        return reg;
    }

    throw std::runtime_error("Too large immediate");
}

void arm64_translation_context::begin_block() {
    ret_ = 0;
    instr_cnt_ = 0;
    builder_ = instruction_builder();
    materialised_nodes_.clear();
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

    // TODO: enable debug mode
    const auto& opcode = disasm.substr(0, 3);
    /* if (opcode == "jnz" || opcode == "tes") { */
    /*     std::cerr << "Adding BRK for instruction " << disasm << '\n'; */
    /*     builder_.brk(immediate_operand(instr_cnt_, 64)); */
    /* } */

    nodes_.clear();
}

void arm64_translation_context::end_instruction() {
    for (const auto* node : nodes_)
        materialise(node);
}

void arm64_translation_context::end_block() {
    // Return value in x0 = 0;
	builder_.mov(preg_operand(preg_operand::x0),
                 mov_immediate(ret_, value_type::u64()));

	do_register_allocation();

	builder_.ret();

	builder_.emit(writer());
}

void arm64_translation_context::lower(ir::node *n) {
    nodes_.push_back(n);
}

void arm64_translation_context::materialise(const ir::node* n) {
    // Invalid node
    if (!n)
        throw std::runtime_error("ARM64 DBT received NULL pointer to node");

    // Avoid materialising again
    if (materialised_nodes_.count(n))
        return;

    switch (n->kind()) {
    case node_kinds::read_reg:
        //std::cerr << "Node: read register\n";
        materialise_read_reg(*reinterpret_cast<const read_reg_node*>(n));
        break;
    case node_kinds::write_reg:
        //std::cerr << "Node: write register\n";
        materialise_write_reg(*reinterpret_cast<const write_reg_node*>(n));
        break;
    case node_kinds::read_mem:
        //std::cerr << "Node: read memory\n";
        materialise_read_mem(*reinterpret_cast<const read_mem_node*>(n));
        break;
    case node_kinds::write_mem:
        //std::cerr << "Node: write memory\n";
        materialise_write_mem(*reinterpret_cast<const write_mem_node*>(n));
        break;
	case node_kinds::read_pc:
        //std::cerr << "Node: read PC\n";
		materialise_read_pc(*reinterpret_cast<const read_pc_node *>(n));
        break;
	case node_kinds::write_pc:
        //std::cerr << "Node: write PC\n";
		materialise_write_pc(*reinterpret_cast<const write_pc_node *>(n));
        break;
    case node_kinds::label:
        //std::cerr << "Node: label\n";
        materialise_label(*reinterpret_cast<const label_node *>(n));
        break;
    case node_kinds::br:
        //std::cerr << "Node: branch\n";
        materialise_br(*reinterpret_cast<const br_node *>(n));
        break;
    case node_kinds::cond_br:
        //std::cerr << "Node: cond branch\n";
        materialise_cond_br(*reinterpret_cast<const cond_br_node *>(n));
        break;
	case node_kinds::cast:
        //std::cerr << "Node: cast\n";
		materialise_cast(*reinterpret_cast<const cast_node *>(n));
        break;
    case node_kinds::csel:
        //std::cerr << "Node: csel\n";
		materialise_csel(*reinterpret_cast<const csel_node *>(n));
        break;
    case node_kinds::bit_shift:
        //std::cerr << "Node: bit shift\n";
		materialise_bit_shift(*reinterpret_cast<const bit_shift_node *>(n));
        break;
    case node_kinds::bit_extract:
        //std::cerr << "Node: bit extract\n";
		materialise_bit_extract(*reinterpret_cast<const bit_extract_node *>(n));
        break;
    case node_kinds::bit_insert:
        //std::cerr << "Node: bit insert\n";
		materialise_bit_insert(*reinterpret_cast<const bit_insert_node *>(n));
        break;
    case node_kinds::constant:
        //std::cerr << "Node: constant\n";
        materialise_constant(*reinterpret_cast<const constant_node*>(n));
        break;
	case node_kinds::unary_arith:
        //std::cerr << "Node: unary arithmetic\n";
        materialise_unary_arith(*reinterpret_cast<const unary_arith_node*>(n));
        break;
	case node_kinds::binary_arith:
        //std::cerr << "Node: binary arithmetic\n";
		materialise_binary_arith(*reinterpret_cast<const binary_arith_node*>(n));
        break;
    case node_kinds::internal_call:
        //std::cerr << "Node: internal call\n";
        materialise_internal_call(*reinterpret_cast<const internal_call_node*>(n));
        break;
    default:
        throw std::runtime_error("unknown node encountered: " +
                                 std::to_string(static_cast<size_t>(n->kind())));
    }

    materialised_nodes_.insert(n);
}

static inline bool is_flag(const port &value) { return value.type().width() == 1; }

static inline bool is_flag_port(const port &value)
{
	return value.kind() == port_kinds::zero || value.kind() == port_kinds::carry || value.kind() == port_kinds::negative
		|| value.kind() == port_kinds::overflow;
}

static inline bool is_gpr(const port &value)
{
	int width = value.type().width();
	return (width == 8 || width == 16 || width == 32 || width == 64) && (!value.type().is_vector()) && value.type().is_integer();
}

void arm64_translation_context::materialise_read_reg(const read_reg_node &n) {
    auto type = n.val().type();

    // Sanity check; cannot by definition load a register larger than 64-bit
    // without it being a vector
    if (type.element_width() > base_type().element_width()) {
        throw std::runtime_error("Larger than 64-bit integers not supported by backend");
    }

    // TODO: implement vector support
    if (type.is_vector()) {
        throw std::runtime_error("Vectors not supported in load routine");
    }

	auto dst_vreg = alloc_vreg_for_port(n.val(), n.val().type());
    builder_.ldr(dst_vreg, guestreg_memory_operand(n.regoff()));
}

void arm64_translation_context::materialise_write_reg(const write_reg_node &n) {
	int w = n.value().type().element_width();

    // TODO: need to implement vector writes
    if (!is_gpr(n.value()) && !is_flag(n.value()))
        throw std::runtime_error("Invalid destination width on register write: "
                                 + std::to_string(w));

    vreg_operand reg;
    if (is_flag(n.value()) && is_flag_port(n.value())) {
        materialise(reinterpret_cast<ir::node*>(n.value().owner()));
        reg = flag_map.at(n.regoff());
    } else {
        reg = vreg_operand_for_port(n.value())[0];
    }

    switch (n.value().type().element_width()) {
        case 1:
            builder_.str(reg, guestreg_memory_operand(n.regoff()));
            break;
        case 8:
            builder_.strb(reg, guestreg_memory_operand(n.regoff()));
            break;
        case 16:
            builder_.strh(reg, guestreg_memory_operand(n.regoff()));
            break;
        case 32:
        case 64:
            // Register set to either 64-bit or 32-bit, stored appropriately with
            // STR
            builder_.str(reg, guestreg_memory_operand(n.regoff()));
            break;
        default:
            // This is by definition; registers >= 64-bits are always vector registers
            throw std::runtime_error("ARM64-DBT cannot write values larger than 64-bits");
    }
}

void arm64_translation_context::materialise_read_mem(const read_mem_node &n) {
	auto dest_vreg = alloc_vreg_for_port(n.val(), n.val().type());

	auto addr_vreg = vreg_operand_for_port(n.address());
    addr_vreg[0] = add_membase(addr_vreg[0]);

    // TODO: widths
    auto mem = memory_operand(addr_vreg[0]);
	builder_.ldr(dest_vreg, mem);
}

void arm64_translation_context::materialise_write_mem(const write_mem_node &n) {
    auto w = n.value().type().element_width();
    if (!is_gpr(n.value()) && !is_flag(n.value())) {
        throw std::runtime_error("Invalid destination width on register write: "
                                 + std::to_string(w));
    }

	auto addr_vreg = add_membase(vreg_operand_for_port(n.address())[0]);
    auto mem = memory_operand(addr_vreg, 0, false, true);

    switch (n.value().type().element_width()) {
        case 1:
            break;
        case 8:
            builder_.strb(vreg_operand_for_port(n.value())[0], mem);
            break;
        case 16:
            builder_.strh(vreg_operand_for_port(n.value())[0], mem);
            break;
        case 32:
        case 64:
            // Register set to either 64-bit or 32-bit, stored appropriately with
            // STR
            builder_.str(vreg_operand_for_port(n.value())[0], mem);
            break;
        default:
            throw std::runtime_error("ARM64-DBT cannot write to main memory values larger than 64-bits");
    }
}

void arm64_translation_context::materialise_read_pc(const read_pc_node &n) {
    // TODO: should this be type() or u64 by default?
	auto dst_vreg = alloc_vreg_for_port(n.val(), n.val().type());

    builder_.mov(dst_vreg, mov_immediate(this_pc_, value_type::u64()));
}

void arm64_translation_context::materialise_write_pc(const write_pc_node &n) {
    auto new_pc_vregs = vreg_operand_for_port(n.value());
    if (new_pc_vregs.size() != 1) {
        throw std::runtime_error("ARM64-DBT does not support PC vregs > 64-bits");
    }

    builder_.str(new_pc_vregs[0],
                 guestreg_memory_operand(static_cast<int>(reg_offsets::PC)));
}

void arm64_translation_context::materialise_label(const label_node &n) {
    if (!builder_.has_label(n.name() + ":"))
        builder_.label(n.name());
}

void arm64_translation_context::materialise_br(const br_node &n) {
    builder_.b(n.target()->name());
}

void arm64_translation_context::materialise_cond_br(const cond_br_node &n) {
    auto cond_vregs = vreg_operand_for_port(n.cond());
    if (cond_vregs.size() != 1) {
        throw std::runtime_error("ARM64-DBT does not support condition vregs > 64-bits");
    }

    auto cond_vreg = cond_vregs[0];
    builder_.cmp(cond_vreg, immediate_operand(0, value_type::u8()));
    builder_.beq(n.target()->name());
}

void arm64_translation_context::materialise_constant(const constant_node &n) {
	auto dst_vreg = alloc_vreg_for_port(n.val(), n.val().type());

    if (n.val().type().is_floating_point()) {
        auto value = n.const_val_f();
        builder_.mov(dst_vreg, mov_immediate(value, n.val().type()));
    } else {
        auto value = n.const_val_i();
        builder_.mov(dst_vreg, mov_immediate(value, n.val().type()));
    }
}

void arm64_translation_context::materialise_binary_arith(const binary_arith_node &n) {
    auto lhs_vregs = vreg_operand_for_port(n.lhs());
    auto rhs_vregs = vreg_operand_for_port(n.rhs());

    if (lhs_vregs.size() != rhs_vregs.size()) {
        throw std::runtime_error("Binary operations not supported with different sized operands");
    }

    for (int i = 0; i < n.val().type().nr_elements(); ++i) {
        alloc_vreg_for_port(n.val(), n.val().type());
    }
	auto dest_vregs = vregs_for_port(n.val());

    size_t dest_width = n.val().type().element_width();
    size_t dest_reg_count = dest_width / base_type().element_width();

    auto lhs_vreg = lhs_vregs[0];
    auto rhs_vreg = rhs_vregs[0];
    auto dest_vreg = dest_vregs[0];

    flag_map[(unsigned long)reg_offsets::ZF] = alloc_vreg_for_port(n.zero(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::SF] = alloc_vreg_for_port(n.negative(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::OF] = alloc_vreg_for_port(n.overflow(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::CF] = alloc_vreg_for_port(n.carry(), value_type::u1());

    // TODO: check
    const char* mod = nullptr;
    if (n.op() == binary_arith_op::add || n.op() == binary_arith_op::sub) {
        switch (n.val().type().element_width()) {
        case 8:
            if (n.val().type().type_class() == value_type_class::signed_integer)
                mod = "SXTB";
            else
                mod = "UXTB";
            break;
        case 16:
            if (n.val().type().type_class() == value_type_class::signed_integer)
                mod = "SXTB";
            else
                mod = "UXTB";
            break;
        }
    }

    const char *cset_type = nullptr;
    switch(n.op()) {
    case binary_arith_op::cmpeq:
        cset_type = "eq";
        break;
    case binary_arith_op::cmpne:
        cset_type = "ne";
        break;
    case binary_arith_op::cmpgt:
        cset_type = "gt";
        break;
    default:
        break;
    }

	switch (n.op()) {
	case binary_arith_op::add:
        if (mod == nullptr)
            builder_.adds(dest_vreg, lhs_vreg, rhs_vreg);
        else
            builder_.adds(dest_vreg, lhs_vreg, rhs_vreg, shift_operand(mod, 0, value_type::u16()));
        for (size_t i = 1; i < dest_reg_count; ++i)
            builder_.adcs(dest_vregs[i], lhs_vregs[i], rhs_vregs[i]);
        break;
	case binary_arith_op::sub:
        if (mod == nullptr)
            builder_.subs(dest_vreg, lhs_vreg, rhs_vreg);
        else
            builder_.subs(dest_vreg, lhs_vreg, rhs_vreg, shift_operand(mod, 0, value_type::u16()));
        for (size_t i = 1; i < dest_reg_count; ++i)
            builder_.sbcs(dest_vregs[i], lhs_vregs[i], rhs_vregs[i]);
        builder_.setcc(flag_map[(unsigned long)reg_offsets::CF]);
        break;
	case binary_arith_op::mul:
        switch (dest_width) {
        case 8:
        case 16:
        case 32:
            switch (n.val().type().type_class()) {
            case ir::value_type_class::signed_integer:
                builder_.smulh(dest_vreg, lhs_vreg, rhs_vreg);
                break;
            case ir::value_type_class::unsigned_integer:
                builder_.umulh(dest_vreg, lhs_vreg, rhs_vreg);
                break;
            case ir::value_type_class::floating_point:
                builder_.fmul(dest_vreg, lhs_vreg, rhs_vreg);
                break;
            case ir::value_type_class::none:
            default:
                throw std::runtime_error("ARM64-DBT encounted unknown type class for multiplication");
            }
            break;
        case 64:
            builder_.mul(dest_vreg, lhs_vreg, rhs_vreg);
            break;
        case 128:
            builder_.mul(dest_vreg, lhs_vreg, rhs_vreg);
            switch (n.val().type().type_class()) {
            case ir::value_type_class::signed_integer:
                builder_.smulh(dest_vregs[1], lhs_vregs[0], rhs_vregs[0]);
                break;
            case ir::value_type_class::unsigned_integer:
                builder_.umulh(dest_vregs[1], lhs_vregs[0], rhs_vregs[0]);
                break;
            case ir::value_type_class::floating_point:
                builder_.fmul(dest_vregs[0], lhs_vregs[0], rhs_vregs[0]);
                break;
            case ir::value_type_class::none:
            default:
                throw std::runtime_error("ARM64-DBT encounted unknown type class for multiplication");
            }
            break;
        case 256:
        case 512:
        default:
            throw std::runtime_error("ARM64-DBT does not support subtraction with sizes larger than 128-bits");
        }

        // *MUL* do not set flags, they must be set here manually
        //
        // FIXME: check correctness for up to 64-bit results
        // FIXME: this is not fully correct for > 64-bit:
        //
        // 1. zero flag must be logically AND-ed between all result registers
        // 2. negative flag must only be considered for the most significant
        // register
        // 3. overflow flag - do we even care?
        // 4. carry flag - how to even determine that for > 64-bit (probably
        // by looking at sources, does x86 even care about it then?)
        //
        // FIXME: this applies to others too
		builder_.cmp(dest_vreg,
                     immediate_operand(0, value_type::u8()));
        break;
	case binary_arith_op::div:
        //FIXME: implement
        builder_.sdiv(dest_vreg, lhs_vreg, rhs_vreg);
        throw std::runtime_error("Not implemented: binary_arith_op::div");
		break;
	case binary_arith_op::mod:
        //FIXME: implement
        builder_.and_(dest_vreg, lhs_vreg, rhs_vreg);
        throw std::runtime_error("Not implemented: binary_arith_op::mov");
		break;
	case binary_arith_op::bor:
        builder_.orr_(dest_vreg, lhs_vreg, rhs_vreg);
        for (size_t i = 1; i < dest_reg_count; ++i) {
            auto flag_vreg_saved = vreg_operand(alloc_vreg(), base_type());
            auto flag_vreg_new = vreg_operand(alloc_vreg(), base_type());
            builder_.msr(flag_vreg_saved, preg_operand(preg_operand::nzcv));

            builder_.orr_(dest_vregs[i], lhs_vregs[i], rhs_vregs[i]);

            builder_.msr(flag_vreg_new, preg_operand(preg_operand::nzcv));
            builder_.orr_(flag_vreg_saved, flag_vreg_saved, flag_vreg_new);
            builder_.msr(preg_operand(preg_operand::nzcv), flag_vreg_saved);
        }
		break;
	case binary_arith_op::band:
        builder_.ands(dest_vreg, lhs_vreg, rhs_vreg);
        for (size_t i = 1; i < dest_reg_count; ++i) {
            auto flag_vreg_saved = vreg_operand(alloc_vreg(), base_type());
            auto flag_vreg_new = vreg_operand(alloc_vreg(), base_type());
            builder_.msr(flag_vreg_saved, preg_operand(preg_operand::nzcv));

            builder_.ands(dest_vregs[i], lhs_vregs[i], rhs_vregs[i]);

            builder_.msr(flag_vreg_new, preg_operand(preg_operand::nzcv));
            builder_.orr_(flag_vreg_saved, flag_vreg_saved, flag_vreg_new);
            builder_.msr(preg_operand(preg_operand::nzcv), flag_vreg_saved);
        }
		break;
	case binary_arith_op::bxor:
        builder_.eor_(dest_vreg, lhs_vreg, rhs_vreg);
        for (size_t i = 1; i < dest_reg_count; ++i)
            builder_.eor_(dest_vregs[i], lhs_vregs[i], rhs_vregs[i]);
        // EOR does not set flags
        // CMP is used to set the flags
        // TODO: find a way to set flags
		builder_.cmp(dest_vreg,
                     immediate_operand(0, value_type::u8()));
        for (size_t i = 1; i < dest_reg_count; ++i) {
            auto flag_vreg_saved = vreg_operand(alloc_vreg(), base_type());
            auto flag_vreg_new = vreg_operand(alloc_vreg(), base_type());
            builder_.msr(flag_vreg_saved, preg_operand(preg_operand::nzcv));
            builder_.cmp(dest_vregs[i],
                         immediate_operand(0, value_type::u8()));
            builder_.msr(flag_vreg_new, preg_operand(preg_operand::nzcv));
            builder_.orr_(flag_vreg_saved, flag_vreg_saved, flag_vreg_new);
            builder_.msr(preg_operand(preg_operand::nzcv), flag_vreg_saved);
        }
		break;
	case binary_arith_op::cmpeq:
	case binary_arith_op::cmpne:
	case binary_arith_op::cmpgt:
        builder_.cmp(lhs_vreg, rhs_vreg);
        for (size_t i = 1; i < dest_reg_count; ++i) {
            auto flag_vreg_saved = vreg_operand(alloc_vreg(), base_type());
            auto flag_vreg_new = vreg_operand(alloc_vreg(), base_type());
            builder_.msr(flag_vreg_saved, preg_operand(preg_operand::nzcv));
            builder_.cmp(dest_vregs[i],
                         immediate_operand(0, value_type::u8()));
            builder_.msr(flag_vreg_new, preg_operand(preg_operand::nzcv));
            builder_.orr_(flag_vreg_saved, flag_vreg_saved, flag_vreg_new);
            builder_.msr(preg_operand(preg_operand::nzcv), flag_vreg_saved);
        }
        builder_.cset(dest_vreg, cond_operand(cset_type));
        for (size_t i = 1; i < dest_reg_count; ++i) {
            builder_.cset(dest_vregs[i], cond_operand(cset_type));
        }
		break;
	default:
		throw std::runtime_error("unsupported binary arithmetic operation " + std::to_string((int)n.op()));
	}

    // FIXME Another write-reg node generated?
	builder_.setz(flag_map[(unsigned long)reg_offsets::ZF]);
	builder_.sets(flag_map[(unsigned long)reg_offsets::SF]);
	builder_.seto(flag_map[(unsigned long)reg_offsets::OF]);

    if (n.op() != binary_arith_op::sub)
        builder_.setc(flag_map[(unsigned long)reg_offsets::CF]);
}

void arm64_translation_context::materialise_unary_arith(const unary_arith_node &n) {
	auto val_vreg = alloc_vreg_for_port(n.val(), n.val().type());

    auto lhs = vreg_operand_for_port(n.lhs())[0];

    switch (n.op()) {
    case unary_arith_op::bnot:
        /* builder_.brk(immediate_operand(100, 64)); */
        if (is_flag(n.val()))
            builder_.eor_(val_vreg, lhs, immediate_operand(1, value_type::u8()));
        else
            builder_.not_(val_vreg, lhs);
        break;
    case unary_arith_op::neg:
        // neg: ~reg + 1 for complement-of-2
        builder_.not_(val_vreg, lhs);
        builder_.add(val_vreg, val_vreg, immediate_operand(1, value_type::u8()));
        break;
    default:
        throw std::runtime_error("Unknown unary operation");
    }
}

void arm64_translation_context::materialise_cast(const cast_node &n) {
    if (n.val().type().is_vector())
        throw std::runtime_error("ARM64-DBT does not support vectors");

    // The implementations of all cast operations depend on 2 things:
    // 1. The width of destination registers (<= 64-bit: the base-width or larger)
    // 2. The width of source registes (<= 64-bit: the base width or larger)
    //
    // However, for extension operations 1 => 2
    size_t src_width = n.source_value().type().element_width();
    size_t dest_width = n.val().type().element_width();

    size_t src_reg_count = src_width / base_type().element_width();
    size_t dest_reg_count = dest_width / base_type().element_width();

    if (src_reg_count == 0) src_reg_count = 1;
    if (dest_reg_count == 0) dest_reg_count = 1;

    // Multiple source registers for element_width > 64-bits
    auto src_vregs = vreg_operand_for_port(n.source_value());

    // Allocate as many destination registers as necessary
    for (size_t i = 0; i < dest_reg_count; ++i) {
        alloc_vreg_for_port(n.val(), n.val().type());
    }
	auto dst_vregs = vregs_for_port(n.val());

    auto src_vreg = src_vregs[0];
    auto dst_vreg = dst_vregs[0];

	switch (n.op()) {
	case cast_op::sx:
        // Sanity check
        if (n.val().type().element_width() <= n.source_value().type().element_width()) {
            throw std::runtime_error("ARM64-DBT cannot sign-extend " +
                    std::to_string(n.val().type().element_width()) + " to smaller size " +
                    std::to_string(n.source_value().type().element_width()));
        }

        // IDEA:
        // 1. Sign-extend reasonably
        // 2. If dest_value > 64-bit, determine sign
        // 3. Plaster sign all over the upper bits
        switch (n.source_value().type().element_width()) {
        case 1:
            // 1 -> N
            // sign-extend to 1 byte
            // sign-extend the rest
            builder_.lsl(dst_vreg, src_vreg, immediate_operand(7, value_type::u8()));
            builder_.sxtb(dst_vreg, src_vreg);
            builder_.asr(dst_vreg, src_vreg, immediate_operand(7, value_type::u8()));
            break;
        case 8:
            builder_.sxtb(dst_vreg, src_vreg);
            break;
        case 16:
            builder_.sxth(dst_vreg, src_vreg);
            break;
        case 32:
            builder_.sxtw(dst_vreg, src_vreg);
            break;
        case 64:
            builder_.mov(dst_vreg, src_vreg);
            break;
        case 128:
        case 256:
            // Move existing values into destination register
            // Sign extension for remaining registers handled outside of the
            // switch
            for (size_t i = 0; i < src_reg_count; ++i) {
                builder_.mov(dst_vregs[i], src_vregs[i]);
            }
            break;
        default:
            throw std::runtime_error("ARM64-DBT cannot sign-extend from size " +
                    std::to_string(src_vreg.width()) + " to size " +
                    std::to_string(dst_vreg.width()));
        }

        // Determine sign and write to upper registers
        // This really only happens when dest_reg_count > src_reg_count > 1
        if (dest_reg_count > 1) {
            for (size_t i = src_reg_count; i < dest_reg_count; ++i) {
                builder_.mov(dst_vregs[i], src_vregs[src_reg_count-1]);
                builder_.asr(dst_vregs[i], dst_vregs[i], immediate_operand(64, value_type::u8()));
            }
        }
        break;
	case cast_op::bitcast:
        // Simply change the meaning of the bit pattern
        // dst_vreg is set to the desired type already, but it must have the
        // value of src_vreg
        // A simple mov is sufficient (eliminated anyway by the register
        // allocator)
        //
        // NOTE: widths guaranteed to be equal by frontend
        if (dest_width != src_width)
            throw std::runtime_error("ARM64-DBT cannot bitcast " +
                    std::to_string(dst_vreg.width()) + " different size " +
                    std::to_string(src_vreg.width()));

        for (size_t i = 0; i < src_reg_count; ++i) {
            builder_.mov(dst_vregs[i], src_vregs[i]);
        }
		break;
	case cast_op::zx:
        // Sanity check
        if (n.val().type().element_width() <= n.source_value().type().element_width()) {
            throw std::runtime_error("ARM64-DBT cannot sign-extend " +
                    std::to_string(n.val().type().element_width()) + " to smaller size " +
                    std::to_string(n.source_value().type().element_width()));
        }

        // IDEA:
        // 1. Sign-extend reasonably
        // 2. If dest_value > 64-bit, determine sign
        // 3. Plaster sign all over the upper bits
        switch (n.source_value().type().element_width()) {
        case 1:
            // 1 -> N
            // sign-extend to 1 byte
            // sign-extend the rest
            builder_.lsl(dst_vreg, src_vreg, immediate_operand(7, value_type::u8()));
            builder_.uxtb(dst_vreg, src_vreg);
            builder_.lsr(dst_vreg, src_vreg, immediate_operand(7, value_type::u8()));
            break;
        case 8:
            builder_.uxtb(dst_vreg, src_vreg);
            break;
        case 16:
            builder_.uxth(dst_vreg, src_vreg);
            break;
        case 32:
            builder_.uxtw(dst_vreg, src_vreg);
            break;
        case 64:
            builder_.mov(dst_vreg, src_vreg);
            break;
        case 128:
        case 256:
            // Handle separately
            for (size_t i = 0; i < src_reg_count; ++i) {
                builder_.mov(dst_vregs[i], src_vregs[i]);
            }
            break;
        default:
            throw std::runtime_error("ARM64-DBT cannot sign-extend from size " +
                    std::to_string(src_vreg.width()) + " to size " +
                    std::to_string(dst_vreg.width()));
        }

        // Determine sign and write to upper registers
        if (dest_reg_count > 1) {
            for (size_t i = src_reg_count; i < dest_reg_count; ++i) {
                builder_.mov(dst_vregs[i], src_vregs[src_reg_count-1]);
                builder_.lsr(dst_vregs[i], dst_vregs[i], immediate_operand(64, value_type::u8()));
            }
        }
        break;
    case cast_op::trunc:
        if (dest_width >= src_width) {
            throw std::runtime_error("ARM64-DBT cannot truncate from " +
                    std::to_string(dst_vreg.width()) + " to larger size " +
                    std::to_string(src_vreg.width()));
        }

        for (size_t i = 0; i < dest_reg_count; ++i) {
            builder_.mov(dst_vregs[i], src_vregs[i]);
        }

        if (is_flag(n.val())) {
            // FIXME: necessary to implement a mov here due to mismatches
            // between types
            //
            // We can end up with a 64-bit dst_vreg and a 32-bit src_vreg
            //
            // Does this even need a fix?
            builder_.and_(src_vreg, src_vreg, immediate_operand(1, value_type::u1()));
            builder_.mov(dst_vreg, src_vreg);
        } else if (src_reg_count == 1) {
            builder_.lsl(dst_vreg, src_vreg, immediate_operand(64 - dest_width, value_type::u64()));
            builder_.asr(dst_vreg, dst_vreg, immediate_operand(64 - dest_width, value_type::u64()));
        }
        break;
    case cast_op::convert:
        // convert between integer and float representations
        if (dest_reg_count != 1) {
            throw std::runtime_error("ARM64-DBT cannot convert " +
                    std::to_string(n.val().type().element_width()) + " because it larger than 64-bit");
        }

        // convert integer to float
        if (n.source_value().type().is_integer() && n.val().type().is_floating_point()) {
             if (n.val().type().type_class() == value_type_class::unsigned_integer) {
                builder_.ucvtf(dst_vreg, src_vreg);
            } else {
                // signed
                builder_.scvtf(dst_vreg, src_vreg);
            }
        } else if (n.source_value().type().is_floating_point() && n.val().type().is_integer()) {
            // Handle float/double -> integer conversions
            switch (n.convert_type()) {
            case fp_convert_type::trunc:
                // if float/double -> truncate to int
                // NOTE: both float/double handled through the same instructions,
                // only register differ
                if (n.val().type().type_class() == value_type_class::unsigned_integer) {
                    builder_.fcvtzu(dst_vreg, src_vreg);
                } else {
                    // signed
                    builder_.fcvtzs(dst_vreg, src_vreg);
                }
                break;
            case fp_convert_type::round:
                // if float/double -> round to closest int
                // NOTE: both float/double handled through the same instructions,
                // only register differ
                if (n.val().type().type_class() == value_type_class::unsigned_integer) {
                    builder_.fcvtau(dst_vreg, src_vreg);
                } else {
                    // signed
                    builder_.fcvtas(dst_vreg, src_vreg);
                }
                break;
            case fp_convert_type::none:
            default:
                throw std::runtime_error("Cannot convert type: " +
                                         std::to_string(int(n.convert_type())));
            }
        } else {
            // converting between different represenations of integers/floating
            // point numbers
            //
            // Destination virtual register set to the correct type upon creation
            // TODO: need to handle different-sized types?
            builder_.mov(dst_vreg, src_vreg);
        }
        break;
	default:
		throw std::runtime_error("unsupported cast operation: "
                                 + to_string(n.op()));
	}
}

void arm64_translation_context::materialise_csel(const csel_node &n) {
    if (n.val().type().is_vector() || n.val().type().element_width() > base_type().element_width()) {
        throw std::runtime_error("ARM64-DBT cannot implement conditional selection for \
                                  vectors and elements widths exceeding 64-bits");
    }

    auto dst_vreg = alloc_vreg_for_port(n.val(), n.val().type());

    auto cond = vreg_operand_for_port(n.condition());

    auto true_val = vreg_operand_for_port(n.trueval());
    auto false_val = vreg_operand_for_port(n.falseval());

    /* builder_.brk(immediate_operand(100, 64)); */
    builder_.cmp(cond[0], immediate_operand(0, value_type::u8()));
    builder_.csel(dst_vreg, true_val[0], false_val[0], cond_operand("NE"));
}

void arm64_translation_context::materialise_bit_shift(const bit_shift_node &n) {
    if (n.val().type().is_vector() || n.val().type().element_width() > base_type().element_width()) {
        throw std::runtime_error("ARM64-DBT cannot implement bit shifts for \
                                  vectors and elements widths exceeding 64-bits");
    }

    auto input = vreg_operand_for_port(n.input())[0];
    auto amount1 = vreg_operand_for_port(n.amount())[0];

    auto dst_type = n.val().type();
    if (n.val().type().element_width() < input.type().element_width()) {
        dst_type = input.type();
    }
    if (dst_type.element_width() < amount1.type().element_width()) {
        dst_type = amount1.type();
    }

    auto amount = vreg_operand(alloc_vreg(), dst_type);
    builder_.mov(amount, amount1);

    auto dst_vreg = alloc_vreg_for_port(n.val(), dst_type);;

    switch (n.op()) {
    case shift_op::lsl:
        builder_.lsl(dst_vreg, input, amount);
        break;
    case shift_op::lsr:
        builder_.lsr(dst_vreg, input, amount);
        break;
    case shift_op::asr:
        builder_.asr(dst_vreg, input, amount);
        break;
    default:
        throw std::runtime_error("unsupported shift operation: " +
                                 std::to_string(static_cast<int>(n.op())));
    }
}

void arm64_translation_context::materialise_bit_extract(const bit_extract_node &n) {
    if (n.val().type().is_vector() || n.val().type().element_width() > base_type().element_width()) {
        throw std::runtime_error("ARM64-DBT cannot implement bit extracts for \
                                  vectors and elements widths exceeding 64-bits");
    }

    auto src_vreg = vreg_operand_for_port(n.source_value())[0];

    // FIXME: 32-bit registers are used as output
    auto dst_type = n.val().type();
    if (n.val().type().element_width() < src_vreg.type().element_width()) {
        dst_type = src_vreg.type();
    }

    auto dst_vreg = alloc_vreg_for_port(n.val(), dst_type);;

    builder_.bfm(dst_vreg, src_vreg,
                  immediate_operand(n.from(), value_type::u8()),
                  immediate_operand(n.length(), value_type::u8()));
}

void arm64_translation_context::materialise_bit_insert(const bit_insert_node &n) {
    if (n.val().type().is_vector() || n.val().type().element_width() > base_type().element_width()) {
        throw std::runtime_error("ARM64-DBT cannot implement bit inserts for \
                                  vectors and elements widths exceeding 64-bits");
    }

    auto dst_vreg = alloc_vreg_for_port(n.val(), n.val().type());

    auto bits_vreg = vreg_operand_for_port(n.bits())[0];

    // FIXME: we end up in a situation that both 64-bit and 32-bit registers are
    // used in the same BFI instruction.
    //
    // We need the equivalent of type promotion.
    auto bits_reg = vreg_operand(alloc_vreg(), dst_vreg.type());
    builder_.mov(bits_reg, bits_vreg);
    builder_.bfi(dst_vreg, bits_reg,
                 immediate_operand(n.to(), value_type::u8()),
                 immediate_operand(n.length(), value_type::u8()));
}

void arm64_translation_context::materialise_internal_call(const internal_call_node &n) {
    // TODO
    // func_push_args(&builder_, n.args());
    // builder_.bl(n.fn().name());
    if (n.fn().name() == "handle_syscall") {
        auto pc_vreg = mov_immediate(this_pc_ + 2, value_type::u64());
        builder_.str(pc_vreg,
                     guestreg_memory_operand(static_cast<int>(reg_offsets::PC)));
        ret_ = 1;
    } else if (n.fn().name() == "handle_int") {
        ret_ = 2;
    } else {
        throw std::runtime_error("unsupported internal call");
    }
}

void arm64_translation_context::do_register_allocation() { builder_.allocate(); }

