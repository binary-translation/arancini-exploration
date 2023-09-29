#include "arancini/ir/node.h"
#include "arancini/ir/port.h"
#include "arancini/ir/value-type.h"
#include "arancini/output/dynamic/arm64/arm64-instruction.h"
#include <arancini/output/dynamic/arm64/arm64-translation-context.h>

#include <arancini/runtime/exec/x86/x86-cpu-state.h>

#include <cmath>
#include <cctype>
#include <string>
#include <cstddef>
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

std::vector<vreg_operand> arm64_translation_context::alloc_vregs(const ir::port &p) {
    auto reg_count = p.type().nr_elements();
    auto element_width = p.type().width();
    if (element_width > base_type().element_width()) {
        // This will only occur for non-vectors > 64-bit
        element_width = base_type().element_width();
        reg_count = p.type().width() / element_width;
    }

    auto type = ir::value_type(p.type().type_class(), element_width, 1);
    for (std::size_t i = 0; i < reg_count; ++i)
        alloc_vreg(p, type);

    auto dest_vregs = vregs_for_port(p);
    return dest_vregs;
}

template <typename T, std::enable_if_t<std::is_arithmetic<T>::value, int>>
vreg_operand arm64_translation_context::mov_immediate(T imm, ir::value_type type) {
    auto actual_size = base_type().element_width() - __builtin_clzll(reinterpret_cast<unsigned long long&>(imm)|1);
    actual_size = std::min(actual_size, type.element_width());

    size_t move_count = static_cast<size_t>(std::ceil(actual_size / 16.0));

    auto immediate = reinterpret_cast<unsigned long long&>(imm);

    // TODO: it seems like the frontend generates very large constants
    // represented as s32(). This fails here, since the resultiing value does not
    // fit
    if (actual_size < 16) {
        auto reg = alloc_vreg(type);
        builder_.mov(reg, immediate_operand(immediate & 0xFFFF, value_type::u16()));
        return reg;
    }

    auto reg = alloc_vreg(type);
    if (actual_size <= base_type().element_width()) {
        builder_.movz(reg,
                      immediate_operand(immediate & 0xFFFF, value_type::u16()),
                      shift_operand("LSL", immediate_operand(0, value_type::u1())));
        for (size_t i = 1; i < move_count; ++i) {
            builder_.movk(reg,
                          immediate_operand(immediate >> (i * 16) & 0xFFFF, value_type::u16()),
                          shift_operand("LSL", immediate_operand(i * 16, value_type::u16())));
        }

        return reg;
    }

    throw std::runtime_error("Too large immediate: " + std::to_string(imm));
}

memory_operand
arm64_translation_context::guestreg_memory_operand(int regoff, bool pre, bool post)
{
    memory_operand mem;
    if (regoff > 255 || regoff < -256) {
        auto preg = context_block_reg;
        auto base_vreg = alloc_vreg(addr_type());
        builder_.mov(base_vreg, immediate_operand(regoff, value_type::u32()));
        builder_.add(base_vreg, preg, base_vreg);
        mem = memory_operand(base_vreg, immediate_operand(0, u12()), pre, post);
    } else {
        auto preg = context_block_reg;
        mem = memory_operand(preg, immediate_operand(regoff, u12()), pre, post);
    }

	return mem;
}

std::vector<vreg_operand> arm64_translation_context::materialise_port(port &p, bool constant_fold) {
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
    auto mem_addr_vreg = alloc_vreg(addr_type());
    builder_.add(mem_addr_vreg, memory_base_reg, addr);

    return mem_addr_vreg;
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
    for (auto node : nodes_)
        materialise(node.get());
}

void arm64_translation_context::end_block() {
    // Return value in x0 = 0;
	builder_.mov(preg_operand(preg_operand::x0),
                 mov_immediate(ret_, value_type::u64()));

    builder_.allocate();

	builder_.ret();

	builder_.emit(writer());
}

void arm64_translation_context::lower(const std::shared_ptr<ir::action_node> &n) {
    nodes_.push_back(n);
}

void arm64_translation_context::materialise(const ir::node* n) {
    // Invalid node
    if (!n)
        throw std::runtime_error("ARM64 DBT received NULL pointer to node");

    // Avoid materialising again
    if (materialised_nodes_.count(n))
        return;

    /* std::cerr << "Handling " << n->to_string() << '\n'; */
    switch (n->kind()) {
    case node_kinds::read_reg:
        materialise_read_reg(*reinterpret_cast<const read_reg_node*>(n));
        break;
    case node_kinds::write_reg:
        materialise_write_reg(*reinterpret_cast<const write_reg_node*>(n));
        break;
    case node_kinds::read_mem:
        materialise_read_mem(*reinterpret_cast<const read_mem_node*>(n));
        break;
    case node_kinds::write_mem:
        materialise_write_mem(*reinterpret_cast<const write_mem_node*>(n));
        break;
	case node_kinds::read_pc:
		materialise_read_pc(*reinterpret_cast<const read_pc_node *>(n));
        break;
	case node_kinds::write_pc:
		materialise_write_pc(*reinterpret_cast<const write_pc_node *>(n));
        break;
    case node_kinds::label:
        materialise_label(*reinterpret_cast<const label_node *>(n));
        break;
    case node_kinds::br:
        materialise_br(*reinterpret_cast<const br_node *>(n));
        break;
    case node_kinds::cond_br:
        materialise_cond_br(*reinterpret_cast<const cond_br_node *>(n));
        break;
	case node_kinds::cast:
		materialise_cast(*reinterpret_cast<const cast_node *>(n));
        break;
    case node_kinds::csel:
		materialise_csel(*reinterpret_cast<const csel_node *>(n));
        break;
    case node_kinds::bit_shift:
		materialise_bit_shift(*reinterpret_cast<const bit_shift_node *>(n));
        break;
    case node_kinds::bit_extract:
		materialise_bit_extract(*reinterpret_cast<const bit_extract_node *>(n));
        break;
    case node_kinds::bit_insert:
		materialise_bit_insert(*reinterpret_cast<const bit_insert_node *>(n));
        break;
    case node_kinds::constant:
        materialise_constant(*reinterpret_cast<const constant_node*>(n));
        break;
	case node_kinds::unary_arith:
        materialise_unary_arith(*reinterpret_cast<const unary_arith_node*>(n));
        break;
	case node_kinds::binary_arith:
		materialise_binary_arith(*reinterpret_cast<const binary_arith_node*>(n));
        break;
    case node_kinds::ternary_arith:
		materialise_ternary_arith(*reinterpret_cast<const ternary_arith_node*>(n));
        break;
	case node_kinds::binary_atomic:
		materialise_binary_atomic(*reinterpret_cast<const binary_atomic_node *>(n));
        break;
	case node_kinds::ternary_atomic:
		materialise_ternary_atomic(*reinterpret_cast<const ternary_atomic_node *>(n));
        break;
    case node_kinds::internal_call:
        materialise_internal_call(*reinterpret_cast<const internal_call_node*>(n));
        break;
	case node_kinds::read_local:
        materialise_read_local(*reinterpret_cast<const read_local_node*>(n));
        break;
	case node_kinds::write_local:
        materialise_write_local(*reinterpret_cast<const write_local_node*>(n));
        break;
    default:
        throw std::runtime_error("unknown node encountered: " +
                                 std::string(n->to_string()));
    }

    materialised_nodes_.insert(n);
}

static inline bool is_flag_port(const port &value) {
	return value.type().width() == 1 || value.kind() == port_kinds::zero ||
           value.kind() == port_kinds::carry || value.kind() == port_kinds::negative ||
           value.kind() == port_kinds::overflow;
}

void arm64_translation_context::materialise_read_reg(const read_reg_node &n) {
    // Sanity check
    auto type = n.val().type();
    if (type.is_vector() && type.element_width() > base_type().element_width()) {
        throw std::runtime_error("[ARM64-DBT] Cannot load vectors with individual elements larger than 64-bits");
    }

    auto dest_vregs = alloc_vregs(n.val());
    for (std::size_t i = 0; i < dest_vregs.size(); ++i) {
        size_t width = dest_vregs[i].type().width();
        auto addr = guestreg_memory_operand(n.regoff() + i * width);
        switch (width) {
            case 1:
            case 8:
                builder_.ldrb(dest_vregs[i], addr);
                break;
            case 16:
                builder_.ldrh(dest_vregs[i], addr);
                break;
            case 32:
            case 64:
                builder_.ldr(dest_vregs[i], addr);
                break;
            default:
                throw std::runtime_error("[ARM64-DBT] cannot load individual register values larger than 64-bits");
        }
    }
}

void arm64_translation_context::materialise_write_reg(const write_reg_node &n) {
    // Sanity check
    auto type = n.val().type();
    if (type.is_vector() && type.element_width() > base_type().element_width()) {
        throw std::runtime_error("[ARM64-DBT] Cannot store vectors with individual elements larger than 64-bits");
    }

    std::vector<vreg_operand> src_vregs = materialise_port(n.value());
    if (is_flag_port(n.value())) {
        src_vregs = {flag_map.at(n.regoff())};
    }

    // FIXME: horrible hack needed here
    //
    // If we have a bit-extract before, we might have casted to a 64-bit type
    // We then attempt to write a single byte, but that won't work because strb()
    // requires a 32-bit register.
    //
    // We now down-cast it.
    //
    // There should be clear type promotion and type coercion.
    for (std::size_t i = 0; i < src_vregs.size(); ++i) {
        if (src_vregs[i].type().width() > n.value().type().width() && n.value().type().width() <= base_type().element_width())
            src_vregs[i].type() = n.value().type();

        size_t width = src_vregs[i].type().width();
        auto addr = guestreg_memory_operand(n.regoff() + i * width);
        switch (width) {
            case 1:
            case 8:
                builder_.strb(src_vregs[i], addr);
                break;
            case 16:
                builder_.strh(src_vregs[i], addr);
                break;
            case 32:
            case 64:
                builder_.str(src_vregs[i], addr);
                break;
            default:
                // This is by definition; registers >= 64-bits are always vector registers
                throw std::runtime_error("[ARM64-DBT] cannot write individual register values larger than 64-bits");
        }
    }
}

void arm64_translation_context::materialise_read_mem(const read_mem_node &n) {
    auto addr_vregs = materialise_port(n.address());
    auto addr_vreg  = addr_vregs[0];

    // Sanity checks
    auto type = n.val().type();
    if (type.is_vector() && type.element_width() > base_type().element_width())
        throw std::runtime_error("[ARM64-DBT] Cannot load vectors from memory with individual elements larger than 64-bits");
    if (addr_vregs.size() != 1)
        throw std::runtime_error("[ARM64-DBT] Only a single address can be specified in a read memory node");

    auto dest_vregs = alloc_vregs(n.val());

    addr_vreg = add_membase(addr_vreg);
    for (std::size_t i = 0; i < dest_vregs.size(); ++i) {
        size_t width = dest_vregs[i].type().width();

        memory_operand mem_op(addr_vreg, immediate_operand(i * width, u12()));
        switch (width) {
            case 1:
            case 8:
                builder_.ldrb(dest_vregs[i], mem_op);
                break;
            case 16:
                builder_.ldrh(dest_vregs[i], mem_op);
                break;
            case 32:
            case 64:
                builder_.ldr(dest_vregs[i], mem_op);
                break;
            default:
                // This is by definition; registers >= 64-bits are always vector registers
                throw std::runtime_error("[ARM64-DBT] cannot load individual memory values larger than 64-bits");
        }
    }
}

void arm64_translation_context::materialise_write_mem(const write_mem_node &n) {
    auto addr_vregs = materialise_port(n.address());

    auto addr_vreg = addr_vregs[0];
    auto type = n.val().type();

    // Sanity check; cannot by definition load a register larger than 64-bit
    // without it being a vector
    if (type.is_vector() && type.element_width() > base_type().element_width())
        throw std::runtime_error("Larger than 64-bit integers in vectors not supported by backend");
    if (addr_vregs.size() != 1)
        throw std::runtime_error("[ARM64-DBT] Only a single address can be specified in a write memory node");

    addr_vreg = add_membase(addr_vreg);
    auto src_vregs = materialise_port(n.value());
    for (std::size_t i = 0; i < src_vregs.size(); ++i) {
        size_t width = src_vregs[i].type().width();

        memory_operand mem_op(addr_vreg, immediate_operand(i * width, u12()));
        switch (width) {
            case 1:
            case 8:
                builder_.strb(src_vregs[i], mem_op);
                break;
            case 16:
                builder_.strh(src_vregs[i], mem_op);
                break;
            case 32:
            case 64:
                builder_.str(src_vregs[i], mem_op);
                break;
            default:
                // This is by definition; registers >= 64-bits are always vector registers
                throw std::runtime_error("[ARM64-DBT] cannot write individual memory values larger than 64-bits");
        }
    }
}

void arm64_translation_context::materialise_read_pc(const read_pc_node &n) {
	auto dest_vreg = alloc_vreg(n.val());
    builder_.mov(dest_vreg, mov_immediate(this_pc_, value_type::u64()));
}

void arm64_translation_context::materialise_write_pc(const write_pc_node &n) {
    auto new_pc_vregs = materialise_port(n.value());
    if (new_pc_vregs.size() != 1) {
        throw std::runtime_error("[ARM64-DBT] Program counter cannot be > 64-bits");
    }

    builder_.str(new_pc_vregs[0],
                 guestreg_memory_operand(static_cast<int>(reg_offsets::PC)));
}

void arm64_translation_context::materialise_label(const label_node &n) {
    if (!builder_.has_label(n.name()))
        builder_.label(n.name());
}

void arm64_translation_context::materialise_br(const br_node &n) {
    builder_.b(n.target()->name());
}

void arm64_translation_context::materialise_cond_br(const cond_br_node &n) {
    auto cond_vregs = materialise_port(n.cond());
    if (cond_vregs.size() != 1) {
        throw std::runtime_error("[ARM64-DBT] Condition vregs for branches cannot be > 64-bits");
    }

    auto cond_vreg = cond_vregs[0];
    builder_.cmp(cond_vreg, immediate_operand(0, value_type::u8()));
    builder_.beq(n.target()->name());
}

void arm64_translation_context::materialise_constant(const constant_node &n) {
	auto dest_vreg = alloc_vreg(n.val());

    if (n.val().type().is_floating_point()) {
        auto value = n.const_val_f();
        builder_.mov(dest_vreg, mov_immediate(value, n.val().type()));
    } else {
        auto value = n.const_val_i();
        builder_.mov(dest_vreg, mov_immediate(value, n.val().type()));
    }
}

void arm64_translation_context::materialise_binary_arith(const binary_arith_node &n) {
    auto lhs_vregs = materialise_port(n.lhs());
    auto rhs_vregs = materialise_port(n.rhs());

    if (lhs_vregs.size() != rhs_vregs.size())
        throw std::runtime_error("[ARM64-DBT] Binary operations not supported with different sized operands");

	auto dest_vregs = alloc_vregs(n.val());
    size_t dest_width = n.val().type().element_width();

    auto lhs_vreg = lhs_vregs[0];
    auto rhs_vreg = rhs_vregs[0];
    auto dest_vreg = dest_vregs[0];

    flag_map[(unsigned long)reg_offsets::ZF] = alloc_vreg(n.zero(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::SF] = alloc_vreg(n.negative(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::OF] = alloc_vreg(n.overflow(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::CF] = alloc_vreg(n.carry(), value_type::u1());

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
            builder_.adds(dest_vreg, lhs_vreg, rhs_vreg, shift_operand(mod, immediate_operand(0, value_type::u16())));
        for (size_t i = 1; i < dest_vregs.size(); ++i)
            builder_.adcs(dest_vregs[i], lhs_vregs[i], rhs_vregs[i]);
        break;
	case binary_arith_op::sub:
        if (mod == nullptr)
            builder_.subs(dest_vreg, lhs_vreg, rhs_vreg);
        else
            builder_.subs(dest_vreg, lhs_vreg, rhs_vreg, shift_operand(mod, immediate_operand(0, value_type::u16())));
        for (size_t i = 1; i < dest_vregs.size(); ++i)
            builder_.sbcs(dest_vregs[i], lhs_vregs[i], rhs_vregs[i]);
        builder_.setcc(flag_map[(unsigned long)reg_offsets::CF]);
        break;
	case binary_arith_op::mul:
        switch (dest_width) {
        case 8:
        case 16:
        case 32:
            // TODO: replace this with NEON
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
                throw std::runtime_error("[ARM64-DBT] encounted unknown type class for multiplication");
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
                throw std::runtime_error("[ARM64-DBT] encounted unknown type class for multiplication");
            }
            break;
        case 256:
        case 512:
        default:
            throw std::runtime_error("[ARM64-DBT] does not support subtraction with sizes larger than 128-bits");
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
        /* throw std::runtime_error("Not implemented: binary_arith_op::div"); */
		break;
	case binary_arith_op::mod:
        //FIXME: implement
        builder_.and_(dest_vreg, lhs_vreg, rhs_vreg);
        /* throw std::runtime_error("Not implemented: binary_arith_op::mov"); */
		break;
	case binary_arith_op::bor:
        builder_.orr_(dest_vreg, lhs_vreg, rhs_vreg);
		break;
	case binary_arith_op::band:
        builder_.ands(dest_vreg, lhs_vreg, rhs_vreg);
		break;
	case binary_arith_op::bxor:
        builder_.eor_(dest_vreg, lhs_vreg, rhs_vreg);
        for (size_t i = 1; i < dest_vregs.size(); ++i)
            builder_.eor_(dest_vregs[i], lhs_vregs[i], rhs_vregs[i]);
        // EOR does not set flags
        // CMP is used to set the flags
        // TODO: find a way to set flags
		builder_.cmp(dest_vreg,
                     immediate_operand(0, value_type::u8()));
		break;
	case binary_arith_op::cmpeq:
	case binary_arith_op::cmpne:
	case binary_arith_op::cmpgt:
        builder_.cmp(lhs_vreg, rhs_vreg);
        builder_.cset(dest_vreg, cond_operand(cset_type));
        for (size_t i = 1; i < dest_vregs.size(); ++i) {
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

void arm64_translation_context::materialise_ternary_arith(const ternary_arith_node &n) {
    flag_map[(unsigned long)reg_offsets::ZF] = alloc_vreg(n.zero(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::SF] = alloc_vreg(n.negative(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::OF] = alloc_vreg(n.overflow(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::CF] = alloc_vreg(n.carry(), value_type::u1());

	auto dest_vregs = alloc_vregs(n.val());
    auto lhs_vregs = materialise_port(n.lhs());
    auto rhs_vregs = materialise_port(n.rhs());
    auto top_vregs = materialise_port(n.top());

    if (dest_vregs.size() != lhs_vregs.size() ||
        dest_vregs.size() != rhs_vregs.size() ||
        dest_vregs.size() != top_vregs.size()) {
        throw std::runtime_error("[ARM64-DBT] Ternary arithmetic node mismatch between types");
    }

    const char* mod = nullptr;
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

    auto pstate = alloc_vreg(preg_operand(preg_operand::nzcv).type());
    for (std::size_t i = 0; i < dest_vregs.size(); ++i) {
        // Set carry flag
        builder_.mrs(pstate, preg_operand(preg_operand::nzcv));
        builder_.lsl(top_vregs[i], top_vregs[i], immediate_operand(0x3, value_type::u8()));
        top_vregs[i].type() = pstate.type();
        builder_.orr_(pstate, pstate, top_vregs[i]);
        builder_.msr(preg_operand(preg_operand::nzcv), pstate);

        switch (n.op()) {
        case ternary_arith_op::adc:
            if (mod)
                builder_.adcs(dest_vregs[i], lhs_vregs[i], rhs_vregs[i], shift_operand(mod, {0, value_type::u16()}));
            else
                builder_.adcs(dest_vregs[i], lhs_vregs[i], rhs_vregs[i]);
            break;
        case ternary_arith_op::sbb:
            if (mod)
                builder_.sbcs(dest_vregs[i], lhs_vregs[i], rhs_vregs[i], shift_operand(mod, {0, value_type::u16()}));
            else
                builder_.sbcs(dest_vregs[i], lhs_vregs[i], rhs_vregs[i]);
            break;
        default:
            throw std::runtime_error("unsupported ternary arithmetic operation " + std::to_string((int)n.op()));
        }
    }

	builder_.setz(flag_map[(unsigned long)reg_offsets::ZF]);
	builder_.sets(flag_map[(unsigned long)reg_offsets::SF]);
	builder_.seto(flag_map[(unsigned long)reg_offsets::OF]);
    builder_.setc(flag_map[(unsigned long)reg_offsets::CF]);
}

void arm64_translation_context::materialise_binary_atomic(const binary_atomic_node &n) {
	auto dest_vreg = vregs_for_port(n.val())[0];

    auto src_vreg = materialise_port(n.rhs())[0];

    // TODO: handle flags with atomics
    flag_map[(unsigned long)reg_offsets::ZF] = alloc_vreg(n.zero(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::SF] = alloc_vreg(n.negative(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::OF] = alloc_vreg(n.overflow(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::CF] = alloc_vreg(n.carry(), value_type::u1());

    auto addr_reg = vregs_for_port(n.lhs())[0];
    auto mem_addr = memory_operand(add_membase(addr_reg));

    // FIXME: correct memory ordering?
    // NOTE: not sure if the proper alternative was used (should a/al/l or
    // nothing be used?)
	switch (n.op()) {
	case binary_atomic_op::add:
	case binary_atomic_op::sub:
        if (n.op() == binary_atomic_op::sub)
            builder_.neg(src_vreg, src_vreg);
        switch (n.val().type().element_width()) {
        case 8:
            builder_.ldaddb(src_vreg, dest_vreg, mem_addr);
            break;
        case 16:
            builder_.ldaddh(src_vreg, dest_vreg, mem_addr);
            break;
        case 32:
            builder_.ldaddw(src_vreg, dest_vreg, mem_addr);
            break;
        case 64:
            builder_.ldadd(src_vreg, dest_vreg, mem_addr);
            break;
        default:
            throw std::runtime_error("Atomic LDADD cannot handle sizes > 64-bit");
        }
        if (n.op() == binary_atomic_op::sub)
            builder_.setcc(flag_map[(unsigned long)reg_offsets::CF]);
        break;
	case binary_atomic_op::bor:
        switch (n.val().type().element_width()) {
        case 8:
            builder_.ldsetb(src_vreg, dest_vreg, mem_addr);
            break;
        case 16:
            builder_.ldseth(src_vreg, dest_vreg, mem_addr);
            break;
        case 32:
            builder_.ldsetw(src_vreg, dest_vreg, mem_addr);
            break;
        case 64:
            builder_.ldset(src_vreg, dest_vreg, mem_addr);
            break;
        default:
            throw std::runtime_error("Atomic LDSET cannot handle sizes > 64-bit");
        }
		break;
	case binary_atomic_op::band:
        // TODO: Not sure if this is correct
        builder_.not_(src_vreg, src_vreg);
        switch (n.val().type().element_width()) {
        case 8:
            builder_.ldclrb(src_vreg, dest_vreg, mem_addr);
            break;
        case 16:
            builder_.ldclrh(src_vreg, dest_vreg, mem_addr);
            break;
        case 32:
            builder_.ldclrw(src_vreg, dest_vreg, mem_addr);
            break;
        case 64:
            builder_.ldclr(src_vreg, dest_vreg, mem_addr);
            break;
        default:
            throw std::runtime_error("Atomic LDCLR cannot handle sizes > 64-bit");
        }
		break;
	case binary_atomic_op::bxor:
        switch (n.val().type().element_width()) {
        case 8:
            builder_.ldeorb(src_vreg, dest_vreg, mem_addr);
            break;
        case 16:
            builder_.ldeorh(src_vreg, dest_vreg, mem_addr);
            break;
        case 32:
            builder_.ldeorw(src_vreg, dest_vreg, mem_addr);
            break;
        case 64:
            builder_.ldeor(src_vreg, dest_vreg, mem_addr);
            break;
        default:
            throw std::runtime_error("Atomic LDEOR cannot handle sizes > 64-bit");
        }
		break;
    case binary_atomic_op::btc:
        switch (n.val().type().element_width()) {
        case 8:
            builder_.ldclrb(src_vreg, dest_vreg, mem_addr);
            break;
        case 16:
            builder_.ldclrh(src_vreg, dest_vreg, mem_addr);
            break;
        case 32:
            builder_.ldclrw(src_vreg, dest_vreg, mem_addr);
            break;
        case 64:
            builder_.ldclr(src_vreg, dest_vreg, mem_addr);
            break;
        default:
            throw std::runtime_error("Atomic LDCLR cannot handle sizes > 64-bit");
        }
		break;
    case binary_atomic_op::bts:
        switch (n.val().type().element_width()) {
        case 8:
            builder_.ldsetb(src_vreg, dest_vreg, mem_addr);
            break;
        case 16:
            builder_.ldseth(src_vreg, dest_vreg, mem_addr);
            break;
        case 32:
            builder_.ldsetw(src_vreg, dest_vreg, mem_addr);
            break;
        case 64:
            builder_.ldset(src_vreg, dest_vreg, mem_addr);
            break;
        default:
            throw std::runtime_error("Atomic LDSET cannot handle sizes > 64-bit");
        }
		break;
    case binary_atomic_op::xadd:
        // TODO: this is more or less what the slow case would look like
        // It should be implemented as a wrapper over an operation
        //
        // The actual wrapper can be used also for implementing the equivalent of
        // a lock to enable actual atomicity
        {
            // TODO: must find a way to make this unique
            std::string restart_label = "restart";
            builder_.label(restart_label);
            switch(n.val().type().element_width()) {
            case 8:
                builder_.ldaxrb(dest_vreg, mem_addr);
                break;
            case 16:
                builder_.ldaxrh(dest_vreg, mem_addr);
                break;
            case 32:
                builder_.ldaxrw(dest_vreg, mem_addr);
                break;
            case 64:
                builder_.ldaxr(dest_vreg, mem_addr);
                break;
            default:
                throw std::runtime_error("Atomic XADD not supported for sizes > 64-bit");
            }
            builder_.adds(dest_vreg, dest_vreg, src_vreg);

            auto status = alloc_vreg(value_type::u32());
            switch(n.val().type().element_width()) {
            case 8:
                builder_.stlxrb(status, dest_vreg, mem_addr);
                break;
            case 16:
                builder_.stlxrh(status, dest_vreg, mem_addr);
                break;
            case 32:
                builder_.stlxrw(status, dest_vreg, mem_addr);
                break;
            case 64:
                builder_.stlxr(status, dest_vreg, mem_addr);
                break;
            default:
                throw std::runtime_error("Atomic XADD not supported for sizes > 64-bit");
            }

            builder_.cbnz(status, restart_label);
        }
        break;
    case binary_atomic_op::xchg:
        // TODO: check if this works
        switch(n.val().type().element_width()) {
        case 8:
            builder_.swpb(dest_vreg, src_vreg, mem_addr);
            break;
        case 16:
            builder_.swph(dest_vreg, src_vreg, mem_addr);
            break;
        case 32:
            builder_.swpw(dest_vreg, src_vreg, mem_addr);
            break;
        case 64:
            builder_.swp(dest_vreg, src_vreg, mem_addr);
            break;
        default:
            throw std::runtime_error("Atomic XCHG not supported for sizes > 64-bit");
        }
        break;
	default:
		throw std::runtime_error("unsupported binary atomic operation " + std::to_string((int)n.op()));
	}

    // FIXME Another write-reg node generated?
	builder_.setz(flag_map[(unsigned long)reg_offsets::ZF]);
	builder_.sets(flag_map[(unsigned long)reg_offsets::SF]);
	builder_.seto(flag_map[(unsigned long)reg_offsets::OF]);

    if (n.op() != binary_atomic_op::sub)
        builder_.setc(flag_map[(unsigned long)reg_offsets::CF]);
}

void arm64_translation_context::materialise_ternary_atomic(const ternary_atomic_node &n) {
	auto dest_vreg = vregs_for_port(n.val())[0];

    auto src_vreg = materialise_port(n.rhs())[0];

    // TODO: handle flags with atomics
    flag_map[(unsigned long)reg_offsets::ZF] = alloc_vreg(n.zero(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::SF] = alloc_vreg(n.negative(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::OF] = alloc_vreg(n.overflow(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::CF] = alloc_vreg(n.carry(), value_type::u1());

    auto addr_reg = vregs_for_port(n.lhs())[0];
    auto mem_addr = memory_operand(add_membase(addr_reg));

    switch (n.op()) {
    case ternary_atomic_op::cmpxchg:
 /* LDXR   Rtemp, [Rmem]      // Load the current value of memory location [Rmem] into Rtemp */
 /*    CMP    Rtemp, Rcmp        // Compare Rtemp with the expected value (Rcmp) */
 /*    B.NE   .failure           // If they aren't the same, branch to failure */
 /*    STXR   Rstatus, Rnew, [Rmem] // Try to store the new value (Rnew) into [Rmem] */
 /*    CBZ    Rstatus, .success  // If the store was successful (Rstatus == 0), branch to success */
 /*    B      .loop */
    case ternary_atomic_op::adc:
    case ternary_atomic_op::sbb:
    default:
		throw std::runtime_error("unsupported binary atomic operation " + std::to_string((int)n.op()));
    }
}

void arm64_translation_context::materialise_unary_arith(const unary_arith_node &n) {
	auto val_vreg = alloc_vreg(n.val());

    auto lhs = materialise_port(n.lhs())[0];

    switch (n.op()) {
    case unary_arith_op::bnot:
        /* builder_.brk(immediate_operand(100, 64)); */
        if (is_flag_port(n.val()))
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
        throw std::runtime_error("[ARM64-DBT] does not support vectors");

    // The implementations of all cast operations depend on 2 things:
    // 1. The width of destination registers (<= 64-bit: the base-width or larger)
    // 2. The width of source registes (<= 64-bit: the base width or larger)
    //
    // However, for extension operations 1 => 2

    // Multiple source registers for element_width > 64-bits
    auto src_vregs = materialise_port(n.source_value());

    // Allocate as many destination registers as necessary
    // TODO: this is not exactly correct, since we need to create different
    // registers of the base type in such cases
    auto type = n.val().type();

    // Sanity check; cannot by definition load a register larger than 64-bit
    // without it being a vector
    if (type.is_vector() && type.element_width() > base_type().element_width()) {
        throw std::runtime_error("Larger than 64-bit integers in vectors not supported by backend");
    }

    auto dest_vregs = alloc_vregs(n.val());

    auto src_vreg = src_vregs[0];
    auto dest_vreg = dest_vregs[0];

	switch (n.op()) {
	case cast_op::sx:
        // Sanity check
        if (n.val().type().element_width() <= n.source_value().type().element_width()) {
            throw std::runtime_error("[ARM64-DBT] cannot sign-extend " +
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
            builder_.lsl(dest_vreg, src_vreg, immediate_operand(7, value_type::u8()));
            builder_.sxtb(dest_vreg, src_vreg);
            builder_.asr(dest_vreg, src_vreg, immediate_operand(7, value_type::u8()));
            break;
        case 8:
            builder_.sxtb(dest_vreg, src_vreg);
            break;
        case 16:
            builder_.sxth(dest_vreg, src_vreg);
            break;
        case 32:
            builder_.sxtw(dest_vreg, src_vreg);
            break;
        case 64:
            builder_.mov(dest_vreg, src_vreg);
            break;
        case 128:
        case 256:
            // Move existing values into destination register
            // Sign extension for remaining registers handled outside of the
            // switch
            for (size_t i = 0; i < src_vregs.size(); ++i) {
                builder_.mov(dest_vregs[i], src_vregs[i]);
            }
            break;
        default:
            throw std::runtime_error("[ARM64-DBT] cannot sign-extend from size " +
                    std::to_string(src_vreg.width()) + " to size " +
                    std::to_string(dest_vreg.width()));
        }

        // Determine sign and write to upper registers
        // This really only happens when dest_reg_count > src_reg_count > 1
        if (dest_vregs.size() > 1) {
            for (size_t i = src_vregs.size(); i < dest_vregs.size(); ++i) {
                builder_.mov(dest_vregs[i], src_vregs[src_vregs.size()-1]);
                builder_.asr(dest_vregs[i], dest_vregs[i], immediate_operand(64, value_type::u8()));
            }
        }
        break;
	case cast_op::bitcast:
        // Simply change the meaning of the bit pattern
        // dest_vreg is set to the desired type already, but it must have the
        // value of src_vreg
        // A simple mov is sufficient (eliminated anyway by the register
        // allocator)
        //
        // NOTE: widths guaranteed to be equal by frontend
        if (dest_vreg.width() != src_vreg.width())
            throw std::runtime_error("[ARM64-DBT] cannot bitcast " +
                    std::to_string(dest_vreg.width()) + " different size " +
                    std::to_string(src_vreg.width()));

        for (size_t i = 0; i < src_vregs.size(); ++i) {
            builder_.mov(dest_vregs[i], src_vregs[i]);
        }
		break;
	case cast_op::zx:
        // Sanity check
        if (n.val().type().element_width() <= n.source_value().type().element_width()) {
            throw std::runtime_error("[ARM64-DBT] cannot sign-extend " +
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
            builder_.lsl(dest_vreg, src_vreg, immediate_operand(7, value_type::u8()));
            builder_.uxtb(dest_vreg, src_vreg);
            builder_.lsr(dest_vreg, src_vreg, immediate_operand(7, value_type::u8()));
            break;
        case 8:
            builder_.uxtb(dest_vreg, src_vreg);
            break;
        case 16:
            builder_.uxth(dest_vreg, src_vreg);
            break;
        case 32:
        case 64:
            builder_.mov(dest_vreg, src_vreg);
            break;
        case 128:
        case 256:
            // Handle separately
            for (size_t i = 0; i < src_vregs.size(); ++i) {
                builder_.mov(dest_vregs[i], src_vregs[i]);
            }
            break;
        default:
            throw std::runtime_error("[ARM64-DBT] cannot sign-extend from size " +
                    std::to_string(src_vreg.width()) + " to size " +
                    std::to_string(dest_vreg.width()));
        }

        // Determine sign and write to upper registers
        if (dest_vregs.size() > 1) {
            for (size_t i = src_vregs.size(); i < dest_vregs.size(); ++i) {
                builder_.mov(dest_vregs[i], src_vregs[src_vregs.size()-1]);
                builder_.lsr(dest_vregs[i], dest_vregs[i], mov_immediate(64, value_type::u64()));
            }
        }
        break;
    case cast_op::trunc:
        if (dest_vreg.width() > src_vreg.width()) {
            throw std::runtime_error("[ARM64-DBT] cannot truncate from " +
                    std::to_string(dest_vreg.width()) + " to larger size " +
                    std::to_string(src_vreg.width()));
        }

        for (size_t i = 0; i < dest_vregs.size(); ++i) {
            builder_.mov(dest_vregs[i], src_vregs[i]);
        }

        if (is_flag_port(n.val())) {
            // FIXME: necessary to implement a mov here due to mismatches
            // between types
            //
            // We can end up with a 64-bit dest_vreg and a 32-bit src_vreg
            //
            // Does this even need a fix?
            builder_.and_(src_vreg, src_vreg, immediate_operand(1, value_type::u1()));
            builder_.mov(dest_vreg, src_vreg);
        } else if (src_vregs.size() == 1) {
            // TODO: again register reallocation problems, this should be clearly
            // specified as a smaller size
            auto immediate = mov_immediate(64 - dest_vreg.width(), value_type::u64());
            builder_.lsl(dest_vreg, src_vreg, immediate);
            builder_.asr(dest_vreg, dest_vreg, immediate);
        }
        break;
    case cast_op::convert:
        // convert between integer and float representations
        if (dest_vregs.size() != 1) {
            throw std::runtime_error("[ARM64-DBT] cannot convert " +
                    std::to_string(n.val().type().element_width()) + " because it larger than 64-bit");
        }

        // convert integer to float
        if (n.source_value().type().is_integer() && n.val().type().is_floating_point()) {
             if (n.val().type().type_class() == value_type_class::unsigned_integer) {
                builder_.ucvtf(dest_vreg, src_vreg);
            } else {
                // signed
                builder_.scvtf(dest_vreg, src_vreg);
            }
        } else if (n.source_value().type().is_floating_point() && n.val().type().is_integer()) {
            // Handle float/double -> integer conversions
            switch (n.convert_type()) {
            case fp_convert_type::trunc:
                // if float/double -> truncate to int
                // NOTE: both float/double handled through the same instructions,
                // only register differ
                if (n.val().type().type_class() == value_type_class::unsigned_integer) {
                    builder_.fcvtzu(dest_vreg, src_vreg);
                } else {
                    // signed
                    builder_.fcvtzs(dest_vreg, src_vreg);
                }
                break;
            case fp_convert_type::round:
                // if float/double -> round to closest int
                // NOTE: both float/double handled through the same instructions,
                // only register differ
                if (n.val().type().type_class() == value_type_class::unsigned_integer) {
                    builder_.fcvtau(dest_vreg, src_vreg);
                } else {
                    // signed
                    builder_.fcvtas(dest_vreg, src_vreg);
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
            builder_.mov(dest_vreg, src_vreg);
        }
        break;
	default:
		throw std::runtime_error("unsupported cast operation: "
                                 + to_string(n.op()));
	}
}

void arm64_translation_context::materialise_csel(const csel_node &n) {
    if (n.val().type().is_vector() || n.val().type().element_width() > base_type().element_width()) {
        throw std::runtime_error("[ARM64-DBT] cannot implement conditional selection for \
                                  vectors and elements widths exceeding 64-bits");
    }

    auto dest_vreg = alloc_vreg(n.val());

    auto cond = materialise_port(n.condition());

    auto true_val = materialise_port(n.trueval());
    auto false_val = materialise_port(n.falseval());

    /* builder_.brk(immediate_operand(100, 64)); */
    builder_.cmp(cond[0], immediate_operand(0, value_type::u8()));
    builder_.csel(dest_vreg, true_val[0], false_val[0], cond_operand("NE"));
}

void arm64_translation_context::materialise_bit_shift(const bit_shift_node &n) {
    if (n.val().type().is_vector() || n.val().type().element_width() > base_type().element_width()) {
        throw std::runtime_error("[ARM64-DBT] cannot implement bit shifts for \
                                  vectors and elements widths exceeding 64-bits");
    }

    auto input = materialise_port(n.input())[0];
    auto amount1 = materialise_port(n.amount())[0];

    auto dest_type = n.val().type();
    if (n.val().type().element_width() < input.type().element_width()) {
        dest_type = input.type();
    }
    if (dest_type.element_width() < amount1.type().element_width()) {
        dest_type = amount1.type();
    }

    auto amount = alloc_vreg(dest_type);
    builder_.mov(amount, amount1);

    auto dest_vreg = alloc_vreg(n.val(), dest_type);;

    switch (n.op()) {
    case shift_op::lsl:
        builder_.lsl(dest_vreg, input, amount);
        break;
    case shift_op::lsr:
        builder_.lsr(dest_vreg, input, amount);
        break;
    case shift_op::asr:
        builder_.asr(dest_vreg, input, amount);
        break;
    default:
        throw std::runtime_error("unsupported shift operation: " +
                                 std::to_string(static_cast<int>(n.op())));
    }
}

static inline std::size_t total_width(const std::vector<vreg_operand> &vec) {
    return std::ceil(vec.size() * vec[0].width());
}

void arm64_translation_context::materialise_bit_extract(const bit_extract_node &n) {
    auto src_vregs = materialise_port(n.source_value());
    auto dest_vregs = alloc_vregs(n.val());

    // Sanity check
    if (dest_vregs.size() > src_vregs.size())
        throw std::runtime_error("[ARM64-DBT] Destination cannot be larger than source for bit extract node");

    auto dest_total_width = total_width(dest_vregs);
    auto src_total_width = total_width(src_vregs);
    auto extract_start = n.from() / src_total_width;

    std::size_t extracted = 0;
    auto extract_idx = n.from() % src_vregs[0].width();
    auto extract_len = std::min(src_vregs[0].width() - extract_idx, n.length() - extracted);

    std::size_t dest_idx = 0;
    for (std::size_t i = extract_start; extracted < n.length(); ++i) {
        dest_vregs[dest_idx].type() = src_vregs[i].type();
        builder_.bfxil(dest_vregs[dest_idx], src_vregs[i],
                      immediate_operand(extract_idx, value_type::u8()),
                      immediate_operand(extract_len, value_type::u8()));
        extract_idx = 0;
        extracted += extract_len;
        extract_len = std::min(n.length() - extracted, src_vregs[i].width());
        dest_idx = extracted % dest_total_width;
    }
}

void arm64_translation_context::materialise_bit_insert(const bit_insert_node &n) {
    auto bits_vregs = materialise_port(n.bits());
    auto src_vregs  = materialise_port(n.source_value());

    auto dest_vregs = alloc_vregs(n.val());

    // Sanity check
    if (dest_vregs.size() != src_vregs.size())
        throw std::runtime_error("[ARM64-DBT] Source and destination mismatch for bit insert node");

    // Copy source to dest
    for (std::size_t i = 0; i < dest_vregs.size(); ++i) {
        builder_.mov(dest_vregs[i], src_vregs[i]);
    }

    auto dest_total_width = total_width(dest_vregs);
    auto insert_start = n.to() / dest_total_width;

    std::size_t inserted = 0;
    std::size_t insert_idx = n.to() % dest_vregs[0].width();
    std::size_t insert_len = std::min(dest_vregs[0].width() - insert_idx, n.length() - inserted);

    std::size_t bits_idx = 0;
    std::size_t bits_total_width = total_width(bits_vregs);
    for (std::size_t i = insert_start; inserted < n.length(); ++i) {
        auto bits_vreg_width = bits_vregs[bits_idx].width();
        bits_vregs[bits_idx].type() = dest_vregs[i].type();
        builder_.bfi(dest_vregs[i], bits_vregs[bits_idx],
                     immediate_operand(insert_idx, value_type::u8()),
                     immediate_operand(insert_len, value_type::u8()));
        insert_idx = 0;
        inserted += insert_len;
        insert_len = std::min(n.length() - inserted, bits_vreg_width);
        bits_idx = inserted % bits_total_width;
    }
}

void arm64_translation_context::materialise_internal_call(const internal_call_node &n) {
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

void arm64_translation_context::materialise_read_local(const read_local_node &n) {
    auto dest_vreg = alloc_vreg(n.val(), n.val().type());
    auto local = locals_[n.local()][0];

    builder_.mov(dest_vreg, local);
}

void arm64_translation_context::materialise_write_local(const write_local_node &n) {
    vreg_operand dest_vreg;
    auto write_reg = vregs_for_port(n.write_value())[0];
    if (locals_.count(n.local()) == 0) {
        dest_vreg = alloc_vreg(n.val(), n.val().type());
        locals_[n.local()] = {dest_vreg};
    } else {
        dest_vreg = locals_[n.local()][0];
    }

    builder_.mov(dest_vreg, write_reg);
}

