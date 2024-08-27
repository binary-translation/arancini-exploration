#include <arancini/ir/node.h>
#include <arancini/ir/port.h>
#include <arancini/ir/value-type.h>
#include <arancini/input/registers.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>
#include <arancini/util/type-utils.h>
#include <arancini/output/dynamic/arm64/arm64-translation-context.h>

#include <arancini/runtime/exec/x86/x86-cpu-state.h>

#include <cmath>
#include <cctype>
#include <string>
#include <cstddef>
#include <unordered_map>

using namespace arancini::output::dynamic::arm64;
using namespace arancini::ir;

// TODO: move to common
register_operand memory_base_reg(register_operand::x18);
register_operand context_block_reg(register_operand::x29);

// TODO: handle as part of capabilities code
static constexpr bool supports_lse = false;

// TODO: these should not be hardcoded
const register_operand ZF(register_operand::x10);
const register_operand CF(register_operand::x11);
const register_operand OF(register_operand::x12);
const register_operand SF(register_operand::x13);

using arancini::input::x86::reg_offsets;

static std::unordered_map<unsigned long, register_operand> flag_map {
	{ (unsigned long)reg_offsets::ZF, {} },
	{ (unsigned long)reg_offsets::CF, {} },
	{ (unsigned long)reg_offsets::OF, {} },
	{ (unsigned long)reg_offsets::SF, {} },
};

// TODO: should be handled as part of capabilities code
static value_type addr_type() {
    return value_type::u64();
}

static value_type base_type() {
    return value_type::u64();
}

template <typename T>
static immediate_operand clamped_immediate(T v, value_type t) {
    return immediate_operand(v & ~(1 << t.element_width()), t);
}

register_sequence& virtual_register_allocator::allocate_sequence(const ir::port &p) {
    std::size_t reg_count = p.type().nr_elements();
    auto element_width = p.type().width();
    if (element_width > base_type().width()) {
        element_width = base_type().width();
        reg_count = p.type().width() / element_width;
    }

    auto type = ir::value_type(p.type().type_class(), element_width, 1);

    std::vector<register_operand> regset;
    for (std::size_t i = 0; i < reg_count; ++i) {
        auto reg = allocate(type);
        regset.push_back(reg);
    }

    port_to_vreg_.emplace(&p, register_sequence(regset.begin(), regset.end()));

    return port_to_vreg_[&p];
}

register_operand arm64_translation_context::cast(const register_operand &op, value_type type) {
    auto dest_vreg = vreg_alloc_.allocate(type);
    builder_.mov(dest_vreg, op);
    return dest_vreg;
}

template <typename T, std::enable_if_t<std::is_arithmetic<T>::value, int>>
register_operand arm64_translation_context::mov_immediate(T imm, ir::value_type type) {
    // TODO: this is wrong (casting to larger value than actual value)
    auto actual_size = base_type().element_width() - __builtin_clzll(reinterpret_cast<unsigned long long&>(imm)|1);
    actual_size = std::min(actual_size, type.element_width());

    std::size_t move_count = static_cast<std::size_t>(std::ceil(actual_size / 16.0));

    auto immediate = reinterpret_cast<unsigned long long&>(imm);

    if (actual_size < 16) {
        builder_.insert_comment("Move immediate directly as < 16-bits");
        auto reg = vreg_alloc_.allocate(type);
        builder_.mov(reg, immediate_operand(immediate & 0xFFFF, value_type::u16()));
        return reg;
    }

    auto reg = vreg_alloc_.allocate(type);
    if (actual_size <= base_type().element_width()) {
        builder_.insert_comment("Move immediate directly as > 16-bits");
        builder_.movz(reg,
                      immediate_operand(immediate & 0xFFFF, value_type::u16()),
                      shift_operand("LSL", immediate_operand(0, value_type::u1())));
        for (std::size_t i = 1; i < move_count; ++i) {
            builder_.movk(reg,
                          immediate_operand(immediate >> (i * 16) & 0xFFFF, value_type::u16()),
                          shift_operand("LSL", immediate_operand(i * 16, value_type::u16())));
        }

        return reg;
    }

    throw backend_exception("Too large immediate: {}", imm);
}

memory_operand
arm64_translation_context::guestreg_memory_operand(int regoff, memory_operand::address_mode mode)
{
    memory_operand mem;
    if (regoff > 255 || regoff < -256) {
        const register_operand& base_vreg = vreg_alloc_.allocate(addr_type());
        builder_.mov(base_vreg, immediate_operand(regoff, value_type::u32()));
        builder_.add(base_vreg, context_block_reg, base_vreg);
        mem = memory_operand(base_vreg, immediate_operand(0, u12()), mode);
    } else {
        mem = memory_operand(context_block_reg, immediate_operand(regoff, u12()), mode);
    }

	return mem;
}

register_operand arm64_translation_context::add_membase(const register_operand &addr, const value_type &type) {
    const register_operand& mem_addr_vreg = vreg_alloc_.allocate(type);
    builder_.add(mem_addr_vreg, memory_base_reg, addr, "add memory base register");

    return mem_addr_vreg;
}

void arm64_translation_context::begin_block() {
    ret_ = 0;
    instr_cnt_ = 0;
    builder_ = instruction_builder();
    materialised_nodes_.clear();
}

[[nodiscard]]
inline std::string labelify(std::string_view original_label) {
    std::string label(original_label.size(), 'a');

    std::transform(original_label.begin(), original_label.end(),
                   label.begin(), [](const auto& c) {
                        if (std::ispunct(c) || std::isspace(c))
                            return '_';
                        return c;
                   });

    return label;
}

void arm64_translation_context::begin_instruction(off_t address, const std::string &disasm) {
	instruction_index_to_guest_[builder_.nr_instructions()] = address;

    current_instruction_disasm_ = disasm;

	this_pc_ = address;
    logger.debug("Translating instruction {} at address {:#x}\n", disasm, address);

    // This should be done optionally
    instr_cnt_++;
    builder_.insert_separator(fmt::format("instruction_{}", instr_cnt_), disasm);

    nodes_.clear();
}

void arm64_translation_context::end_instruction() {
    try {
        for (const auto* node : nodes_)
            materialise(node);
    } catch (std::exception &e) {
        logger.error("{}\n", util::logging_separator());
        logger.error("Instruction translation failed for instruction {} with translation:\n{}\n",
                     current_instruction_disasm_,
                     fmt::format("{}", fmt::join(builder_.instruction_begin(), builder_.instruction_end(), "\n")));
        logger.error("{}\n", util::logging_separator());
        throw backend_exception("Instruction translation failed: {}", e.what());
    }
}

void arm64_translation_context::end_block() {
    // Return value in x0 = 0;
	builder_.mov(register_operand(register_operand::x0),
                 mov_immediate(ret_, value_type::u64()));

    try {
        builder_.allocate();

        builder_.ret();

        builder_.emit(writer());
    } catch (std::exception &e) {
        // TODO: views as lvalues
        logger.error("{}\n", util::logging_separator());
        logger.error("Register allocation failed for instruction {} with translation:\n{}\n",
                     current_instruction_disasm_,
                     fmt::format("{}", fmt::join(builder_.instruction_begin(), builder_.instruction_end(), "\n")));
        logger.error("{}\n", util::logging_separator());
        throw backend_exception("Register allocation failed: {}", e.what());
    }

    // Reset context for next block of instructions
    reset_context();
}

void arm64_translation_context::reset_context() {
    nodes_.clear();
    materialised_nodes_.clear();
    vreg_alloc_.reset();
    instruction_index_to_guest_.clear();
    locals_.clear();
}

void arm64_translation_context::lower(const std::shared_ptr<ir::action_node> &n) {
    nodes_.push_back(n.get());
}

void arm64_translation_context::materialise(const ir::node* n) {
    // Invalid node
    if (!n)
        throw backend_exception("Received NULL pointer to node when materialising");

    // Avoid materialising again
    if (materialised_nodes_.count(n))
        return;

    logger.debug("Handling {}\n", n->kind());
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
    case node_kinds::vector_insert:
		materialise_vector_insert(*reinterpret_cast<const vector_insert_node *>(n));
        break;
    case node_kinds::vector_extract:
		materialise_vector_extract(*reinterpret_cast<const vector_extract_node *>(n));
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
        throw backend_exception("Unknown node encountered with index {}", util::to_underlying(n->kind()));
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
        throw backend_exception("Cannot load vectors with individual elements larger than 64-bits");
    }

    auto comment = fmt::format("read register: {}", n.regname());

    auto &dest_vregs = vreg_alloc_.allocate(n.val());
    for (std::size_t i = 0; i < dest_vregs.size(); ++i) {
        std::size_t width = dest_vregs[i].type().width();
        auto addr = guestreg_memory_operand(n.regoff() + i * width);
        switch (width) {
            case 1:
            case 8:
                builder_.ldrb(dest_vregs[i], addr, comment);
                break;
            case 16:
                builder_.ldrh(dest_vregs[i], addr, comment);
                break;
            case 32:
            case 64:
                builder_.ldr(dest_vregs[i], addr, comment);
                break;
            default:
                throw backend_exception("cannot load individual register values larger than 64-bits");
        }
    }
}

void arm64_translation_context::materialise_write_reg(const write_reg_node &n) {
    // Sanity check
    auto type = n.val().type();
    if (type.is_vector() && type.element_width() > base_type().element_width())
        throw backend_exception("Cannot store vectors with individual elements larger than 64-bits");

    auto &src_vregs = materialise_port(n.value());
    if (is_flag_port(n.value())) {
        const auto &src_vreg = flag_map.at(n.regoff());
        auto addr = guestreg_memory_operand(n.regoff());
        builder_.strb(src_vreg, addr, fmt::format("write flag: {}", n.regname()));
        return;
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
    std::string comment("write register: ");
    comment += n.regname();

    for (std::size_t i = 0; i < src_vregs.size(); ++i) {
        if (src_vregs[i].type().width() > n.value().type().width() && n.value().type().width() <= base_type().element_width())
            src_vregs[i] = cast(src_vregs[i], n.value().type());

        std::size_t width = src_vregs[i].type().width();
        auto addr = guestreg_memory_operand(n.regoff() + i * width);
        switch (width) {
            case 1:
            case 8:
                builder_.strb(src_vregs[i], addr, comment);
                break;
            case 16:
                builder_.strh(src_vregs[i], addr, comment);
                break;
            case 32:
            case 64:
                builder_.str(src_vregs[i], addr, comment);
                break;
            default:
                // This is by definition; registers >= 64-bits are always vector registers
                throw backend_exception("Cannot write individual register values larger than 64-bits");
        }
    }
}

void arm64_translation_context::materialise_read_mem(const read_mem_node &n) {
    const register_operand &addr_vreg = materialise_port(n.address());

    // Sanity checks
    auto type = n.val().type();
    if (type.is_vector() && type.element_width() > base_type().element_width())
        throw backend_exception("Cannot load vectors from memory with individual elements larger than 64-bits");

    const auto &dest_vregs = vreg_alloc_.allocate(n.val());

    auto comment = "read memory";
    for (std::size_t i = 0; i < dest_vregs.size(); ++i) {
        std::size_t width = dest_vregs[i].type().width();

        memory_operand mem_op(addr_vreg, immediate_operand(i * width, u12()));
        switch (width) {
            case 1:
            case 8:
                builder_.ldrb(dest_vregs[i], mem_op, comment);
                break;
            case 16:
                builder_.ldrh(dest_vregs[i], mem_op, comment);
                break;
            case 32:
            case 64:
                builder_.ldr(dest_vregs[i], mem_op, comment);
                break;
            default:
                // This is by definition; registers >= 64-bits are always vector registers
                throw backend_exception("Cannot load individual memory values larger than 64-bits");
        }
    }
}

void arm64_translation_context::materialise_write_mem(const write_mem_node &n) {
    const auto &addr_vreg = materialise_port(n.address());

    auto type = n.val().type();

    // Sanity check; cannot by definition load a register larger than 64-bit
    // without it being a vector
    if (type.is_vector() && type.element_width() > base_type().element_width())
        throw backend_exception("Larger than 64-bit integers in vectors not supported by backend");

    const auto &address = add_membase(addr_vreg);
    const auto &src_vregs = materialise_port(n.value());

    auto comment = "write memory";
    for (std::size_t i = 0; i < src_vregs.size(); ++i) {
        std::size_t width = src_vregs[i].type().width();

        memory_operand mem_op(address, immediate_operand(i * width, u12()));
        switch (width) {
            case 1:
            case 8:
                builder_.strb(src_vregs[i], mem_op, comment);
                break;
            case 16:
                builder_.strh(src_vregs[i], mem_op, comment);
                break;
            case 32:
            case 64:
                builder_.str(src_vregs[i], mem_op, comment);
                break;
            default:
                // This is by definition; registers >= 64-bits are always vector registers
                throw backend_exception("cannot write individual memory values larger than 64-bits");
        }
    }
}

void arm64_translation_context::materialise_read_pc(const read_pc_node &n) {
	auto dest_vreg = vreg_alloc_.allocate(n.val());
    builder_.mov(dest_vreg, mov_immediate(this_pc_, value_type::u64()), "read program counter");
}

void arm64_translation_context::materialise_write_pc(const write_pc_node &n) {
    const auto &new_pc_vreg = materialise_port(n.value());

    builder_.str(new_pc_vreg,
                 guestreg_memory_operand(static_cast<int>(reg_offsets::PC)),
                 "write program counter");
}

void arm64_translation_context::materialise_label(const label_node &n) {
    if (!builder_.has_label(n.name()))
        builder_.label(n.name());
}

void arm64_translation_context::materialise_br(const br_node &n) {
    builder_.b(n.target()->name());
}

void arm64_translation_context::materialise_cond_br(const cond_br_node &n) {
    const auto &cond_vregs = materialise_port(n.cond());

    builder_.cmp(cond_vregs, immediate_operand(1, value_type::u8()));
    builder_.beq(n.target()->name());
}

void arm64_translation_context::materialise_constant(const constant_node &n) {
	const auto &dest_vreg = vreg_alloc_.allocate(n.val());

    if (n.val().type().is_floating_point()) {
        auto value = n.const_val_f();
        builder_.mov(dest_vreg, mov_immediate(value, n.val().type()), "move float into register");
    } else {
        auto value = n.const_val_i();
        builder_.mov(dest_vreg, mov_immediate(value, n.val().type()), "move integer into register");
    }
}

void arm64_translation_context::materialise_binary_arith(const binary_arith_node &n) {
    const auto &lhs_vregs = materialise_port(n.lhs());
    const auto &rhs_vregs = materialise_port(n.rhs());

    if (lhs_vregs.size() != rhs_vregs.size()) {
        throw backend_exception("Binary operations not supported with different sized operands");
    }

	const auto &dest_vregs = vreg_alloc_.allocate(n.val());
    std::size_t dest_width = n.val().type().element_width();

    const auto &lhs_vreg = lhs_vregs[0];
    const auto &rhs_vreg = rhs_vregs[0];
    const auto &dest_vreg = dest_vregs[0];

    flag_map[(unsigned long)reg_offsets::ZF] = vreg_alloc_.allocate(n.zero(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::SF] = vreg_alloc_.allocate(n.negative(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::OF] = vreg_alloc_.allocate(n.overflow(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::CF] = vreg_alloc_.allocate(n.carry(), value_type::u1());

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
                mod = "SXTH";
            else
                mod = "UXTH";
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
        if (n.val().type().is_vector()) {
            for (std::size_t i = 0; i < dest_vregs.size(); ++i)
                builder_.adds(dest_vregs[i], lhs_vregs[i], rhs_vregs[i]);
            break;
        }

        if (mod == nullptr)
            builder_.adds(dest_vreg, lhs_vreg, rhs_vreg);
        else
            builder_.adds(dest_vreg, lhs_vreg, rhs_vreg, shift_operand(mod, immediate_operand(0, value_type::u16())));
        for (std::size_t i = 1; i < dest_vregs.size(); ++i)
            builder_.adcs(dest_vregs[i], lhs_vregs[i], rhs_vregs[i]);
        break;
	case binary_arith_op::sub:
        if (mod == nullptr)
            builder_.subs(dest_vreg, lhs_vreg, rhs_vreg);
        else
            builder_.subs(dest_vreg, lhs_vreg, rhs_vreg, shift_operand(mod, immediate_operand(0, value_type::u16())));
        for (std::size_t i = 1; i < dest_vregs.size(); ++i)
            builder_.sbcs(dest_vregs[i], lhs_vregs[i], rhs_vregs[i]);
        builder_.setcc(flag_map[(unsigned long)reg_offsets::CF], "compute flag: CF");
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
                // TODO: note which
                throw backend_exception("Encounted unknown type class for multiplication");
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
                // TODO: note which
                throw backend_exception("Encounted unknown type class for multiplication");
            }
            break;
        case 256:
        case 512:
        default:
            throw backend_exception("Multiplication not support for size {}", dest_width);
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
        /* throw backend_exception("Not implemented: binary_arith_op::div"); */
		break;
	case binary_arith_op::mod:
        //FIXME: implement
        builder_.and_(dest_vreg, lhs_vreg, rhs_vreg);
        /* throw backend_exception("Not implemented: binary_arith_op::mov"); */
		break;
	case binary_arith_op::bor:
        builder_.orr_(dest_vreg, lhs_vreg, rhs_vreg);
		break;
	case binary_arith_op::band:
        builder_.ands(dest_vreg, lhs_vreg, rhs_vreg);
		break;
	case binary_arith_op::bxor:
        builder_.eor_(dest_vreg, lhs_vreg, rhs_vreg);
        for (std::size_t i = 1; i < dest_vregs.size(); ++i)
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
        builder_.cmp(lhs_vreg, rhs_vreg, "compare LHS and RHS to generate condition for conditional set");
        builder_.cset(dest_vreg, cond_operand(cset_type), "set to 1 if condition is true (based flags from the previous compare)");
        for (std::size_t i = 1; i < dest_vregs.size(); ++i) {
            builder_.cset(dest_vregs[i], cond_operand(cset_type));
        }
		break;
	default:
		throw backend_exception("Unsupported binary arithmetic operation with index {}", util::to_underlying(n.op()));
	}

    // FIXME Another write-reg node generated?
	builder_.setz(flag_map[(unsigned long)reg_offsets::ZF], "compute flag: ZF");
	builder_.sets(flag_map[(unsigned long)reg_offsets::SF], "compute flag: SF");
	builder_.seto(flag_map[(unsigned long)reg_offsets::OF], "compute flag: OF");

    if (n.op() != binary_arith_op::sub)
        builder_.setc(flag_map[(unsigned long)reg_offsets::CF], "compute flag: CF");
}

void arm64_translation_context::materialise_ternary_arith(const ternary_arith_node &n) {
    flag_map[(unsigned long)reg_offsets::ZF] = vreg_alloc_.allocate(n.zero(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::SF] = vreg_alloc_.allocate(n.negative(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::OF] = vreg_alloc_.allocate(n.overflow(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::CF] = vreg_alloc_.allocate(n.carry(), value_type::u1());

	const auto &dest_vregs = vreg_alloc_.allocate(n.val());
    const auto &lhs_vregs = materialise_port(n.lhs());
    const auto &rhs_vregs = materialise_port(n.rhs());
    auto &top_vregs = materialise_port(n.top());

    if (dest_vregs.size() != lhs_vregs.size() ||
        dest_vregs.size() != rhs_vregs.size() ||
        dest_vregs.size() != top_vregs.size()) {
        throw backend_exception("Ternary arithmetic node mismatch between types");
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

    const register_operand& pstate = vreg_alloc_.allocate(register_operand(register_operand::nzcv).type());
    for (std::size_t i = 0; i < dest_vregs.size(); ++i) {
        // Set carry flag
        builder_.mrs(pstate, register_operand(register_operand::nzcv));
        builder_.lsl(top_vregs[i], top_vregs[i], immediate_operand(0x3, value_type::u8()));

        cast(top_vregs[i], pstate.type());

        builder_.orr_(pstate, pstate, top_vregs[i]);
        builder_.msr(register_operand(register_operand::nzcv), pstate);

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
            throw backend_exception("Unsupported ternary arithmetic operation {}", util::to_underlying(n.op()));
        }
    }

	builder_.setz(flag_map[(unsigned long)reg_offsets::ZF], "compute flag: ZF");
	builder_.sets(flag_map[(unsigned long)reg_offsets::SF], "compute flag: SF");
	builder_.seto(flag_map[(unsigned long)reg_offsets::OF], "compute flag: OF");
    builder_.setc(flag_map[(unsigned long)reg_offsets::CF], "compute flag: CF");
}

void arm64_translation_context::materialise_binary_atomic(const binary_atomic_node &n) {
	const auto &dest_vregs = vreg_alloc_.get(n.val());
    const auto &src_vregs = materialise_port(n.rhs());
    const auto &addr_regs = vreg_alloc_.get(n.address());

    if (addr_regs.size() != 1)
        throw backend_exception("Binary atomic operation address cannot be > 64-bits");
    if (dest_vregs.size() != 1 || src_vregs.size() != 1)
        throw backend_exception("Binary atomic operations not supported for vectors");

    const auto &src_vreg = src_vregs[0];
    const auto &dest_vreg = dest_vregs[0];

    // No need to handle flags: they are not visible to other PEs
    flag_map[(unsigned long)reg_offsets::ZF] = vreg_alloc_.allocate(n.zero(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::SF] = vreg_alloc_.allocate(n.negative(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::OF] = vreg_alloc_.allocate(n.overflow(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::CF] = vreg_alloc_.allocate(n.carry(), value_type::u1());

    memory_operand mem_addr(add_membase(addr_regs[0]));

    // FIXME: correct memory ordering?
    // NOTE: not sure if the proper alternative was used (should a/al/l or
    // nothing be used?)
	switch (n.op()) {
	case binary_atomic_op::add:
	case binary_atomic_op::sub:
        if (n.op() == binary_atomic_op::sub)
            builder_.neg(src_vreg, src_vreg);
        switch (n.val().type().element_width()) {
        case 1:
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
            throw backend_exception("Atomic LDADD cannot handle sizes > 64-bit");
        }
        if (n.op() == binary_atomic_op::sub)
            builder_.setcc(flag_map[(unsigned long)reg_offsets::CF]);
        break;
	case binary_atomic_op::bor:
        switch (n.val().type().element_width()) {
        case 1:
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
            throw backend_exception("Atomic LDSET cannot handle sizes > 64-bit");
        }
		break;
	case binary_atomic_op::band:
        // TODO: Not sure if this is correct
        builder_.not_(src_vreg, src_vreg);
        switch (n.val().type().element_width()) {
        case 1:
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
            throw backend_exception("Atomic LDCLR cannot handle sizes > 64-bit");
        }
		break;
	case binary_atomic_op::bxor:
        switch (n.val().type().element_width()) {
        case 1:
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
            throw backend_exception("Atomic LDEOR cannot handle sizes > 64-bit");
        }
		break;
    case binary_atomic_op::btc:
        switch (n.val().type().element_width()) {
        case 1:
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
            throw backend_exception("Atomic LDCLR cannot handle sizes > 64-bit");
        }
		break;
    case binary_atomic_op::bts:
        switch (n.val().type().element_width()) {
        case 1:
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
            throw backend_exception("Atomic LDSET cannot handle sizes > 64-bit");
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
            builder_.insert_comment("Atomic addition (load atomically, add and retry if failed)");

            std::string restart_label = "restart";
            builder_.label(restart_label, "set label for jump (needed in case of restarting operation)");
            switch(n.val().type().element_width()) {
            case 1:
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
                throw backend_exception("Atomic XADD not supported for sizes > 64-bit");
            }
            builder_.adds(dest_vreg, dest_vreg, src_vreg, "perform addition with loaded source");

            auto status = vreg_alloc_.allocate(value_type::u32());
            switch(n.val().type().element_width()) {
            case 1:
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
                throw backend_exception("Atomic XADD not supported for sizes > 64-bit");
            }

            builder_.cbnz(status, restart_label, "write back source or restart operation (by jumping to label)");
        }
        break;
    case binary_atomic_op::xchg:
        // TODO: check if this works
        switch(n.val().type().element_width()) {
        case 1:
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
            throw backend_exception("Atomic XCHG not supported for sizes > 64-bit");
        }
        break;
	default:
		throw backend_exception("unsupported binary atomic operation {}", util::to_underlying(n.op()));
	}

	builder_.setz(flag_map[(unsigned long)reg_offsets::ZF], "write flag: ZF");
	builder_.sets(flag_map[(unsigned long)reg_offsets::SF], "write flag: SF");
	builder_.seto(flag_map[(unsigned long)reg_offsets::OF], "write flag: OF");

    if (n.op() != binary_atomic_op::sub)
        builder_.setc(flag_map[(unsigned long)reg_offsets::CF], "write flag: CF");
}

void arm64_translation_context::materialise_ternary_atomic(const ternary_atomic_node &n) {
    // TODO: this is completely wrong
    // FIXME
    const register_operand &dest_vreg = vreg_alloc_.allocate(n.val());
    const register_operand &acc_vreg = materialise_port(n.rhs());
    const register_operand &src_vreg = materialise_port(n.top());
    const register_operand &addr_vreg = materialise_port(n.address());

    flag_map[(unsigned long)reg_offsets::ZF] = vreg_alloc_.allocate(n.zero(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::SF] = vreg_alloc_.allocate(n.negative(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::OF] = vreg_alloc_.allocate(n.overflow(), value_type::u1());
    flag_map[(unsigned long)reg_offsets::CF] = vreg_alloc_.allocate(n.carry(), value_type::u1());

    auto mem_addr = memory_operand(add_membase(addr_vreg));

    // CMPXCHG:
    // dest_vreg = mem(mem_base + addr);
    // if (dest_vreg != acc_vregs) acc_vregs = dest_vregs;
    // else try_write(mem_base + addr) = src_vreg;
    //      if (retry) goto beginning
    // end
    switch (n.op()) {
    case ternary_atomic_op::cmpxchg:
        if constexpr (supports_lse) {
            builder_.insert_comment("Atomic CMPXCHG using CAS (enabled on systems with LSE support");
            builder_.cas(acc_vreg, src_vreg, memory_operand(mem_addr),
                          "write source to memory if source == accumulator, accumulator = source");
            builder_.mov(acc_vreg, dest_vreg, "move result of accumulator into destination register");
        } else {
            builder_.insert_comment("Atomic CMPXCHG without CAS");
            builder_.label("loop");
            builder_.ldxr(dest_vreg, memory_operand(mem_addr), "load atomically");
            builder_.cmp(dest_vreg, acc_vreg, "compare with accumulator");
            builder_.bne(label_operand("failure"), "if loaded value != accumulator branch to failure");
            builder_.stxr(dest_vreg, src_vreg, memory_operand(mem_addr), "store if not failure");
            builder_.cbz(dest_vreg, label_operand("success"), "!= 0 represents success storing");
            builder_.label("failure");
            builder_.add(acc_vreg, dest_vreg, immediate_operand(0, acc_vreg.type()));
            builder_.b(label_operand("loop"), "loop until failure or success");
            builder_.label("success");
        }
        break;
    case ternary_atomic_op::adc:
    case ternary_atomic_op::sbb:
    default:
		throw backend_exception("unsupported binary atomic operation {}", util::to_underlying(n.op()));
    }
}

void arm64_translation_context::materialise_unary_arith(const unary_arith_node &n) {
    const auto &dest_vregs = vreg_alloc_.allocate(n.val());
    const auto &lhs_vregs = materialise_port(n.lhs());

    if (dest_vregs.size() != 1 || lhs_vregs.size() != 1)
        throw backend_exception("Unary arithmetic node does not support operations > 64-bit");

    const auto &dest_vreg = dest_vregs[0];
    const auto &lhs_vreg = lhs_vregs[0];

    switch (n.op()) {
    case unary_arith_op::bnot:
        /* builder_.brk(immediate_operand(100, 64)); */
        if (is_flag_port(n.val()))
            builder_.eor_(dest_vreg, lhs_vreg, immediate_operand(1, value_type::u8()));
        else
            builder_.not_(dest_vreg, lhs_vreg);
        break;
    case unary_arith_op::neg:
        // neg: ~reg + 1 for complement-of-2
        builder_.not_(dest_vreg, lhs_vreg);
        builder_.add(dest_vreg, dest_vreg, immediate_operand(1, value_type::u8()));
        break;
    default:
        throw backend_exception("Unknown unary operation");
    }
}

void arm64_translation_context::materialise_cast(const cast_node &n) {
    // The implementations of all cast operations depend on 2 things:
    // 1. The width of destination registers (<= 64-bit: the base-width or larger)
    // 2. The width of source registes (<= 64-bit: the base width or larger)
    //
    // However, for extension operations 1 => 2

    // Multiple source registers for element_width > 64-bits
    const auto &src_vregs = materialise_port(n.source_value());

    // Allocate as many destination registers as necessary
    // TODO: this is not exactly correct, since we need to create different
    // registers of the base type in such cases
    const auto &dest_vregs = vreg_alloc_.allocate(n.val());

    const auto &src_vreg = src_vregs[0];
    const auto &dest_vreg = dest_vregs[0];

    logger.debug("Materializing cast operation: {}", n.op());
	switch (n.op()) {
	case cast_op::sx:
        // Sanity check
        if (n.val().type().element_width() <= n.source_value().type().element_width()) {
            throw backend_exception("cannot sign-extend {} to smaller size {}",
                                    n.val().type().element_width(),
                                    n.source_value().type().element_width());
        }

        builder_.insert_comment("sign-extend from {}x{} to {}x{}",
                                n.source_value().type().nr_elements(), n.source_value().type().element_width(),
                                n.val().type().nr_elements(), n.val().type().element_width());

        // IDEA:
        // 1. Sign-extend reasonably
        // 2. If dest_value > 64-bit, determine sign
        // 3. Plaster sign all over the upper bits
        switch (n.source_value().type().element_width()) {
        case 1:
            // 1 -> N
            // sign-extend to 1 byte
            // sign-extend the rest
            builder_.lsl(dest_vreg, src_vreg, immediate_operand(7, value_type::u8()),
                         "shift left LSB to set sign bit of byte");
            builder_.sxtb(dest_vreg, dest_vreg, "sign-extend");
            builder_.asr(dest_vreg, dest_vreg, immediate_operand(7, value_type::u8()),
                         "shift right to fill LSB with sign bit (except for least-significant bit)");
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
            for (std::size_t i = 0; i < src_vregs.size(); ++i)
                builder_.mov(dest_vregs[i], src_vregs[i]);
            break;
        default:
            throw backend_exception("Cannot sign-extend from size {} to size {}",
                                    src_vreg.type().width(),
                                    dest_vreg.type().width());
        }

        // Determine sign and write to upper registers
        // This really only happens when dest_reg_count > src_reg_count > 1
        if (dest_vregs.size() > 1) {
            builder_.insert_comment("Determine sign and write to upper registers");
            for (std::size_t i = src_vregs.size(); i < dest_vregs.size(); ++i) {
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
        builder_.insert_comment("Bitcast from {}x{} to {}x{}",
                                n.source_value().type().nr_elements(), n.source_value().type().element_width(),
                                n.val().type().nr_elements(), n.val().type().element_width());

        if (n.val().type().element_width() > n.source_value().type().element_width()) {
            // Destination consists of fewer elements but of larger widths
            std::size_t dest_idx = 0;
            std::size_t dest_pos = 0;
            for (std::size_t i = 0; i < src_vregs.size(); ++i) {
                const register_operand& src_vreg = vreg_alloc_.allocate(dest_vreg.type());
                builder_.mov(src_vreg, src_vregs[i]);
                builder_.lsl(src_vreg, src_vreg, mov_immediate(dest_pos % n.val().type().element_width(), src_vreg.type()));
                builder_.orr_(dest_vregs[dest_idx], dest_vregs[dest_idx], src_vreg);

                dest_pos += src_vregs[i].type().width();
                dest_idx = (dest_pos / dest_vregs[dest_idx].type().width());
            }
        } else if (n.val().type().element_width() < n.source_value().type().element_width()) {
            // Destination consists of more elements but of smaller widths
            std::size_t src_idx = 0;
            std::size_t src_pos = 0;
            for (std::size_t i = 0; i < dest_vregs.size(); ++i) {
                const register_operand& src_vreg = vreg_alloc_.allocate(dest_vreg.type());
                builder_.mov(src_vreg, src_vregs[src_idx]);
                builder_.lsl(src_vreg, src_vreg, mov_immediate(src_pos % n.source_value().type().element_width(), src_vreg.type()));
                builder_.mov(dest_vregs[i], src_vreg);

                src_pos += src_vregs[i].type().width();
                src_idx = (src_pos / src_vregs[src_idx].type().width());
            }
        } else {
            for (std::size_t i = 0; i < dest_vregs.size(); ++i)
                builder_.mov(dest_vregs[i], src_vregs[i]);
        }
		break;
	case cast_op::zx:
        // Sanity check
        if (n.val().type().element_width() <= n.source_value().type().element_width()) {
            throw backend_exception("Cannot zero-extend {} to smaller size {}",
                                    n.val().type().element_width(),
                                    n.source_value().type().element_width());
        }

        builder_.insert_comment("zero-extend from {}x{} to {}x{}",
                                 n.val().type().nr_elements(), n.val().type().element_width(),
                                n.source_value().type().nr_elements(), n.source_value().type().element_width());

        // IDEA:
        // 1. Zero-extend reasonably
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
            for (std::size_t i = 0; i < src_vregs.size(); ++i) {
                builder_.mov(dest_vregs[i], src_vregs[i]);
            }
            break;
        default:
            throw backend_exception("Cannot sign-extend from size {} to size {}",
                                    src_vreg.type().width(), dest_vreg.type().width());
        }

        // Set upper registers to zero
        builder_.insert_comment("Set upper registers to zero");
        if (dest_vregs.size() > 1) {
            for (std::size_t i = src_vregs.size(); i < dest_vregs.size(); ++i)
                builder_.mov(dest_vregs[i], immediate_operand(0, value_type::u1()));
        }
        break;
    case cast_op::trunc:
        if (dest_vreg.type().element_width() > src_vreg.type().element_width()) {
            throw backend_exception("Cannot truncate from {} to large size {}",
                                    dest_vreg.type().width(), src_vreg.type().width());
        }

        for (std::size_t i = 0; i < dest_vregs.size(); ++i) {
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
            auto immediate = mov_immediate(64 - dest_vreg.type().element_width(), value_type::u64());
            builder_.lsl(dest_vreg, src_vreg, immediate);
            builder_.asr(dest_vreg, dest_vreg, immediate);
        }
        break;
    case cast_op::convert:
        // convert between integer and float representations
        if (dest_vregs.size() != 1) {
            throw backend_exception("Cannot convert {} because it is larger than 64-bits",
                                    n.val().type().element_width());
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
                throw backend_exception("Cannot convert type: {}", util::to_underlying(n.convert_type()));
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
		throw backend_exception("unsupported cast operation with index {}", util::to_underlying(n.op()));
	}
}

void arm64_translation_context::materialise_csel(const csel_node &n) {
    if (n.val().type().is_vector() || n.val().type().element_width() > base_type().element_width()) {
        throw backend_exception("Cannot implement conditional selection for \
                                  vectors and elements widths exceeding 64-bits");
    }

    const auto &dest_vreg = vreg_alloc_.allocate(n.val());
    const auto &cond_vregs = materialise_port(n.condition());
    const auto &true_vregs = materialise_port(n.trueval());
    const auto &false_vregs = materialise_port(n.falseval());

    /* builder_.brk(immediate_operand(100, 64)); */
    builder_.cmp(cond_vregs, immediate_operand(0, value_type::u8()), "compare condition for conditional select");
    builder_.csel(dest_vreg, true_vregs, false_vregs, cond_operand("NE"));
}

void arm64_translation_context::materialise_bit_shift(const bit_shift_node &n) {
    if (n.val().type().is_vector() || n.val().type().element_width() > base_type().element_width()) {
        throw backend_exception("Cannot implement bit shifts for \
                                  vectors and elements widths exceeding 64-bits");
    }

    // TODO: refactor this
    const register_operand &input = materialise_port(n.input());
    const register_operand &amount1 = materialise_port(n.amount());

    auto dest_type = n.val().type();
    if (n.val().type().element_width() < input.type().element_width())
        dest_type = input.type();

    if (dest_type.element_width() < amount1.type().element_width())
        dest_type = amount1.type();

    const register_operand& amount = vreg_alloc_.allocate(dest_type);
    builder_.mov(amount, amount1);

    const register_operand& dest_vreg = vreg_alloc_.allocate(n.val(), dest_type);

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
        throw backend_exception("Unsupported shift operation: {}", n.op());
    }
}

// TODO: this should be part of the register_sequence
static inline std::size_t total_width(const std::vector<register_operand> &vec) {
    return std::ceil(vec.size() * vec[0].type().element_width());
}

void arm64_translation_context::materialise_bit_extract(const bit_extract_node &n) {
    const auto &src_vregs = materialise_port(n.source_value());
    auto &dest_vregs = vreg_alloc_.allocate(n.val());

    // Sanity check
    if (dest_vregs.size() > src_vregs.size())
        throw backend_exception("Destination cannot be larger than source for bit extract node");

    auto dest_total_width = total_width(dest_vregs);
    auto src_total_width = total_width(src_vregs);
    auto extract_start = n.from() / src_total_width;

    // TODO: we shouldn't be using ints here
    std::size_t extracted = 0;
    std::size_t extract_idx = n.from() % src_vregs[0].type().element_width();
    auto extract_len = std::min(src_vregs[0].type().element_width() - extract_idx, n.length() - extracted);

    std::size_t dest_idx = 0;

    builder_.insert_comment("Extract specific bits into destination");
    for (std::size_t i = extract_start; extracted < n.length(); ++i) {
        dest_vregs[dest_idx] = cast(dest_vregs[dest_idx], src_vregs[i].type());

        builder_.bfxil(dest_vregs[dest_idx], src_vregs[i],
                      immediate_operand(extract_idx, value_type::u8()),
                      immediate_operand(extract_len, value_type::u8()));
        extract_idx = 0;
        extracted += extract_len;
        extract_len = std::min(n.length() - extracted, src_vregs[i].type().element_width());
        dest_idx = extracted % dest_total_width;
    }
}

void arm64_translation_context::materialise_bit_insert(const bit_insert_node &n) {
    auto &bits_vregs = materialise_port(n.bits());
    const auto &src_vregs  = materialise_port(n.source_value());
    const auto &dest_vregs = vreg_alloc_.allocate(n.val());

    // Sanity check
    if (dest_vregs.size() != src_vregs.size())
        throw backend_exception("Source and destination mismatch for bit insert node");

    builder_.insert_comment("Bit insert into destination");

    // Copy source to dest
    builder_.insert_comment("Copy source to destination (insertion will overwrite)");
    for (std::size_t i = 0; i < dest_vregs.size(); ++i) {
        builder_.mov(dest_vregs[i], src_vregs[i]);
    }

    auto dest_total_width = total_width(dest_vregs);
    auto insert_start = n.to() / dest_total_width;

    // TODO: We shouldn't be using ints here
    int inserted = 0;
    int insert_idx = n.to() % dest_vregs[0].type().element_width();
    std::size_t insert_len = std::min(dest_vregs[0].type().element_width() - insert_idx, n.length() - inserted);

    std::size_t bits_idx = 0;
    std::size_t bits_total_width = total_width(bits_vregs);

    builder_.insert_comment("insert specific bits into [{}:{}]", n.to()+insert_len, n.to());
    for (std::size_t i = insert_start; inserted < n.length(); ++i) {
        auto bits_vreg_width = bits_vregs[bits_idx].type().element_width();
        bits_vregs[bits_idx] = cast(bits_vregs[bits_idx], dest_vregs[i].type());
        builder_.bfi(dest_vregs[i], bits_vregs[bits_idx],
                     immediate_operand(insert_idx, value_type::u8()),
                     immediate_operand(insert_len, value_type::u8()));
        insert_idx = 0;
        inserted += insert_len;
        insert_len = std::min(n.length() - inserted, bits_vreg_width);
        bits_idx = inserted % bits_total_width;
    }
}

void arm64_translation_context::materialise_vector_insert(const vector_insert_node &n) {
    const auto &dest_vregs = vreg_alloc_.allocate(n.val());
    const auto &src_vregs = materialise_port(n.source_vector());
    const auto &value_vregs = materialise_port(n.insert_value());

    if (dest_vregs.size() < src_vregs.size())
        throw backend_exception("Destination vector for vector insert is smaller than source vector");

    std::size_t index = (n.index() * n.insert_value().type().element_width()) / base_type().element_width();
    if (index + value_vregs.size() > dest_vregs.size())
        throw backend_exception("Cannot insert at index {} in destination vector", index);

    builder_.insert_comment("Insert vector by first copying source to destination");
    for (std::size_t i = 0; i < src_vregs.size(); ++i)
        builder_.mov(dest_vregs[i], src_vregs[i]);

    builder_.insert_comment("Insert value into destination");
    for (std::size_t i = 0; i < value_vregs.size(); ++i) {
        const auto &value_vreg = cast(value_vregs[i], dest_vregs[index + i].type());
        builder_.mov(dest_vregs[index + i], value_vreg);
    }
}

void arm64_translation_context::materialise_vector_extract(const vector_extract_node &n) {
    const auto &dest_vregs = vreg_alloc_.allocate(n.val());
    const auto &src_vregs = materialise_port(n.source_vector());

    std::size_t index = (n.index() * n.source_vector().type().element_width()) / base_type().element_width();
    if (dest_vregs.size() >= src_vregs.size())
        throw backend_exception("Cannot extract vector larger than source vector");
    if (index + dest_vregs.size() > src_vregs.size())
        throw backend_exception("Cannot extract from index {} in source vector", index);

    builder_.insert_comment("Extract vector by copying to destination");
    for (std::size_t i = 0; i < dest_vregs.size(); ++i) {
        const auto &src_vreg = cast(src_vregs[index+i], dest_vregs[i].type());
        builder_.mov(dest_vregs[i], src_vreg);
    }
}

void arm64_translation_context::materialise_internal_call(const internal_call_node &n) {
    if (n.fn().name() == "handle_syscall") {
        auto pc_vreg = mov_immediate(this_pc_ + 2, value_type::u64());
        builder_.str(pc_vreg,
                     guestreg_memory_operand(static_cast<int>(reg_offsets::PC)),
                     "update program counter to handle system call");
        ret_ = 1;
    } else if (n.fn().name() == "handle_int") {
        ret_ = 2;
    } else {
        throw backend_exception("unsupported internal call: {}", n.fn().name());
    }
}

void arm64_translation_context::materialise_read_local(const read_local_node &n) {
    const auto &dest_vregs = vreg_alloc_.allocate(n.val());
    const auto &locals = locals_[n.local()];

    if (locals.size() != dest_vregs.size())
        throw backend_exception("Read local received mismatched types");

    builder_.insert_comment("Read local variable");
    for (std::size_t i = 0; i < dest_vregs.size(); ++i)
        builder_.mov(dest_vregs[i], locals[i]);
}

void arm64_translation_context::materialise_write_local(const write_local_node &n) {
    const auto &write_vregs = vreg_alloc_.get(n.write_value());
    if (locals_.count(n.local()) == 0) {
        const auto &dest_vregs = vreg_alloc_.allocate(n.val());
        locals_[n.local()] = dest_vregs;
    }

    const auto &dest_vregs = locals_[n.local()];
    if (write_vregs.size() != dest_vregs.size())
        throw backend_exception("Write local received mismatched types");

    builder_.insert_comment("Write local variable");
    for (std::size_t i = 0; i < dest_vregs.size(); ++i)
        builder_.mov(dest_vregs[i], dest_vregs[i]);
}

