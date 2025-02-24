#include <arancini/ir/node.h>
#include <arancini/ir/port.h>
#include <arancini/ir/value-type.h>
#include <arancini/input/registers.h>
#include <arancini/util/type-utils.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>
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
register_operand context_block_reg(register_operand::x29);
register_operand dbt_retval_register(register_operand::x0);

// TODO: handle as part of capabilities code
static constexpr bool supports_lse = false;

using arancini::input::x86::reg_offsets;

memory_operand arm64_translation_context::guest_memory(int regoff, memory_operand::address_mode mode) {
    if (regoff > 255 || regoff < -256) {
        const register_operand& base_vreg = vreg_alloc_.allocate(value_types::addr_type);
        builder_.mov(base_vreg, regoff);
        builder_.add(base_vreg, context_block_reg, base_vreg);
        return memory_operand(base_vreg, 0, mode);
    } else {
        return memory_operand(context_block_reg, regoff, mode);
    }
}

void arm64_translation_context::begin_block() {
    ret_ = 0;
    instr_cnt_ = 0;
}

void arm64_translation_context::begin_instruction(off_t address, const std::string &disasm) {
	instruction_index_to_guest_[builder_.size()] = address;

    current_instruction_disasm_ = disasm;

	this_pc_ = address;
    logger.debug("Translating instruction {} at address {:#x}\n", disasm, address);

    instr_cnt_++;

    // TODO: these comments should be inserted only in debug builds
    builder_.insert_comment("instruction_{}: {}", instr_cnt_, disasm);

    nodes_.clear();
}

void arm64_translation_context::end_instruction() {
    try {
        for (const auto* node : nodes_)
            materialise(node);

        // builder_.allocate();
        // builder_.emit(writer());
        // builder_.clear();
    } catch (std::exception &e) {
        logger.error("{}\n", util::logging_separator());
        logger.error("Instruction translation failed for guest instruction '{}' with translation:\n{}\n",
                     current_instruction_disasm_,
                     fmt::format("{}", fmt::join(builder_.instruction_begin(), builder_.instruction_end(), "\n")));
        logger.error("{}\n", util::logging_separator());
        fflush(logger.get_output_file());
        throw backend_exception("Instruction translation failed: {}", e.what());
    }
}

void arm64_translation_context::end_block() {

    try {
        builder_.allocate();

        // These instructions can be inserted after register allocation, since they do not depend on
        // virtual registers and only def()
        builder_.mov(dbt_retval_register, ret_);

        // Return value in x0 = 0;
        builder_.ret();

        builder_.emit(writer());

        builder_.clear();
    } catch (std::exception &e) {
        // TODO: views as lvalues
        logger.error("{}\n", util::logging_separator());
        logger.error("Register allocation failed for guest instruction '{}' with translation:\n{}\n",
                     current_instruction_disasm_,
                     fmt::format("{}", fmt::join(builder_.instruction_begin(), builder_.instruction_end(), "\n")));
        logger.error("{}\n", util::logging_separator());
        fflush(stderr);
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
    [[unlikely]]
    if (!n)
        throw backend_exception("Received NULL pointer to node when materialising");

    // Avoid materialising again
    if (materialised_nodes_.count(n)) {
        logger.debug("Already handled {} node with ID: {}; skipping\n", n->kind(), fmt::ptr(n));
        return;
    }

    logger.debug("Handling {} with node ID: {}\n", n->kind(), fmt::ptr(n));
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
    const auto &dest_vregs = vreg_alloc_.allocate(n.val());

    [[unlikely]]
    if (type.is_vector() && type.element_width() > value_types::base_type.element_width())
        throw backend_exception("Cannot load registers of type {}", type);

    builder_.insert_comment("read register: {}", n.regname());

    builder_.load(dest_vregs, guest_memory(n.regoff()));
}

inline bool is_flag_setter(node_kinds node_kind) {
    return node_kind == node_kinds::binary_arith || node_kind == node_kinds::ternary_arith ||
           node_kind == node_kinds::binary_atomic || node_kind == node_kinds::ternary_atomic;
}

void arm64_translation_context::materialise_write_reg(const write_reg_node &n) {
    // Sanity check
    auto type = n.val().type();
    const auto &source = materialise_port(n.value());

    [[unlikely]]
    if (type.is_vector() && type.element_width() > value_types::base_type.element_width())
        throw backend_exception("Cannot store registers of type {}", type);

    // Flags may be set either based on some preceding operation or with a constant
    // Handle the case when they are generated based on a previous operation here
    if (is_flag_port(n.value())) {
        builder_.insert_comment("write flag: {}", n.regname());
        if (is_flag_setter(n.value().owner()->kind())) {
            const auto &flag = builder_.flag_map().at(static_cast<reg_offsets>(n.regoff()));
            builder_.store(flag, guest_memory(n.regoff()));
        } else {
            builder_.store(source, guest_memory(n.regoff()));
        }
        return;
    }

    builder_.insert_comment("write register: {}", n.regname());
    builder_.store(source, guest_memory(n.regoff()));
}

void arm64_translation_context::materialise_read_mem(const read_mem_node &n) {
    // Sanity checks
    auto type = n.val().type();

    [[unlikely]]
    if (type.is_vector() && type.element_width() > value_types::base_type.element_width())
        throw backend_exception("Cannot load vectors from memory with type {}", type);

    const auto &dest = vreg_alloc_.allocate(n.val());
    const register_operand &address = materialise_port(n.address());
    builder_.load(dest, address);
}

void arm64_translation_context::materialise_write_mem(const write_mem_node &n) {
    auto type = n.val().type();

    // Sanity check; cannot by definition load a register larger than 64-bit
    // without it being a vector
    [[unlikely]]
    if (type.is_vector() && type.element_width() > value_types::base_type.element_width())
        throw backend_exception("Unable to write vectors of type {}", type);

    const auto &src = materialise_port(n.value());

    const auto &address = materialise_port(n.address());
    builder_.store(src, address);
}

void arm64_translation_context::materialise_read_pc(const read_pc_node &n) {
	auto dest = vreg_alloc_.allocate(n.val());
    builder_.mov(dest, this_pc_).add_comment("read program counter");
}

void arm64_translation_context::materialise_write_pc(const write_pc_node &n) {
    const auto &new_pc_vreg = materialise_port(n.value());

	if (n.updates_pc() == br_type::call) {
		ret_ = 3;
	}

	if (n.updates_pc() == br_type::ret) {
		ret_ = 4;
	}

    builder_.insert_comment("update program counter");
    builder_.store(new_pc_vreg, guest_memory(reg_offsets::PC).base_register());
}

void arm64_translation_context::materialise_label(const label_node &n) {
    auto label_name = fmt::format("{}_{}", n.name(), instr_cnt_);
    logger.debug("Inserting label {}\n", label_name);
    auto label = label_operand(label_name);
    builder_.label(label);
}

void arm64_translation_context::materialise_br(const br_node &n) {
    auto label_name = fmt::format("{}_{}", n.target()->name(), instr_cnt_);
    logger.debug("Generating branch to label {}\n", label_name);
    auto label = label_operand(label_name);
    builder_.b(label);
}

void arm64_translation_context::materialise_cond_br(const cond_br_node &n) {
    const auto &cond_vregs = materialise_port(n.cond());

    auto label_name = fmt::format("{}_{}", n.target()->name(), instr_cnt_);
    logger.debug("Generating conditional branch to label {}\n", label_name);
    auto label = label_operand(label_name);
    builder_.cbnz(cond_vregs, label);
}

void arm64_translation_context::materialise_constant(const constant_node &n) {
	const auto &dest = vreg_alloc_.allocate(n.val());

    [[unlikely]]
    if (n.val().type().is_floating_point())
        builder_.fmov(dest, n.const_val_f()).add_comment("move float into register");
    else
        builder_.mov(dest, n.const_val_i()).add_comment("move integer into register");
}

[[nodiscard]]
inline cond_operand get_cset_type(binary_arith_op op) {
    switch(op) {
	case binary_arith_op::cmpueq:
	case binary_arith_op::cmpoeq:
    case binary_arith_op::cmpeq:
        return cond_operand::eq();
	case binary_arith_op::cmpune:
    case binary_arith_op::cmpne:
        return cond_operand::ne();
    case binary_arith_op::cmpgt:
	case binary_arith_op::cmpunle:
        return cond_operand::gt();
	case binary_arith_op::cmpole:
        return cond_operand::le();
	case binary_arith_op::cmpolt:
	case binary_arith_op::cmpult:
	case binary_arith_op::cmpunlt:
        return cond_operand::lt();
	case binary_arith_op::cmpo:
        return cond_operand::vc();
	case binary_arith_op::cmpu:
        return cond_operand::vs();
    default:
        throw backend_exception("Unknown binary operation comparison operation type {}",
                                util::to_underlying(op));
    }
}

void arm64_translation_context::materialise_binary_arith(const binary_arith_node &n) {
    const auto &lhs = materialise_port(n.lhs());
    const auto &rhs = materialise_port(n.rhs());
	auto &destination = vreg_alloc_.allocate(n.val());

    // Sanity check
    // Binary operations are defined in the IR with same size inputs and output
    bool sets_flags = true;
    bool inverse_carry_flag_operation = false;

    builder_.allocate_flags();

    logger.debug("Binary arithmetic node of type {}\n", n.op());
	switch (n.op()) {
	case binary_arith_op::add:
        // Vector addition
        if (n.val().type().is_vector()) {
            sets_flags = false;
            builder_.add(destination, lhs, rhs);
        } else {
            builder_.adds(destination, lhs, rhs);
        }
        break;
	case binary_arith_op::sub:
        // Vector subtraction
        if (n.val().type().is_vector()) {
            sets_flags = false;
            builder_.sub(destination, lhs, rhs);
            break;
        } else {
            builder_.subs(destination, lhs, rhs);
            inverse_carry_flag_operation = true;
        }
        break;
	case binary_arith_op::mul:
        builder_.multiply(destination, lhs, rhs);
        sets_flags = false;
        break;
	case binary_arith_op::div:
        builder_.divide(destination, lhs, rhs);
        sets_flags = false;
		break;
	case binary_arith_op::mod:
        // TODO: this is partially incorrect
        // modulo can be expressed by AND when rhs is 2
        // modulo lhs % rhs is equiv. to lhs % (rhs-1) wheh rhs is a power of 2
        // Generic implementation:
        // lhs % rhs = lhs - (rhs * floor(lhs/rhs))
        {
            builder_.insert_comment("Implementing modulo via division and multiplication");
            auto temp1 = vreg_alloc_.allocate(n.val().type());
            auto temp2 = vreg_alloc_.allocate(n.val().type());
            builder_.divide(temp1, lhs, rhs);
            builder_.multiply(temp2, rhs, temp1);
            builder_.subs(destination, lhs, temp2);
            sets_flags = false;
        }
		break;
	case binary_arith_op::bor:
        builder_.logical_or(destination, lhs, rhs);
        sets_flags = false;
		break;
	case binary_arith_op::band:
        builder_.ands(destination, lhs, rhs);
        sets_flags = false;
		break;
	case binary_arith_op::bxor:
        builder_.exclusive_or(destination, lhs, rhs);
        sets_flags = false;
        break;
	case binary_arith_op::cmpeq:
	case binary_arith_op::cmpne:
	case binary_arith_op::cmpgt:
        // TODO: this looks wrong
        if (n.val().type().is_vector() || destination.size() > 1) {
            throw backend_exception("Unsupported comparison between {} x {}",
                                    n.lhs().type(), n.rhs().type());
        }

        {
            auto typed_rhs = builder_.cast(rhs[0], lhs[0].type());
            builder_.cmp(lhs[0], typed_rhs)
                    .add_comment("compare LHS and RHS to generate condition for conditional set");
            builder_.cset(destination[0], get_cset_type(n.op()))
                    .add_comment("set to 1 if condition is true (based flags from the previous compare)");
            inverse_carry_flag_operation = true;
        }
		break;
	case binary_arith_op::cmpoeq:
	case binary_arith_op::cmpolt:
	case binary_arith_op::cmpole:
	case binary_arith_op::cmpo:
	case binary_arith_op::cmpu:
        builder_.fcmp(lhs, rhs);
        destination[0].cast(value_type::u64());
        builder_.cset(destination[0], get_cset_type(n.op()))
                .add_comment("set to 1 if condition is true (based flags from the previous compare)");
        break;
	case binary_arith_op::cmpueq:
	case binary_arith_op::cmpune:
	case binary_arith_op::cmpult:
	case binary_arith_op::cmpunlt:
	case binary_arith_op::cmpunle:
        {
            builder_.fcmp(lhs, rhs);
            const auto& unordered = vreg_alloc_.allocate(value_type::u64());
            destination[0].cast(value_type::u64());
            builder_.cset(destination[0], get_cset_type(n.op()))
                    .add_comment("set to 1 if condition is true (based flags from the previous compare)");
            builder_.cset(unordered, cond_operand::vs())
                    .add_comment("set to 1 if condition is true (based flags from the previous compare)");
            builder_.logical_or(destination[0], destination[0], unordered[0]);
        }
        break;
	default:
		throw backend_exception("Unsupported binary arithmetic operation with index {}",
                                util::to_underlying(n.op()));
	}

    // Flags are set by most arithmetic operations
    // But not operations on vectors
    [[likely]]
    if (sets_flags) {
        builder_.set_flags(inverse_carry_flag_operation);
    }
}

void arm64_translation_context::materialise_ternary_arith(const ternary_arith_node &n) {
    const auto& lhs = materialise_port(n.lhs());
    const auto& rhs = materialise_port(n.rhs());
    const auto& top = materialise_port(n.top());
	const auto& destination = vreg_alloc_.allocate(n.val());

    bool inverse_carry_flag_operation = false;
    switch (n.op()) {
    case ternary_arith_op::adc:
        builder_.adcs(destination, top, lhs, rhs);
        break;
    case ternary_arith_op::sbb:
        builder_.sbcs(destination, top, lhs, rhs);
        inverse_carry_flag_operation = true;
        break;
    default:
        throw backend_exception("Unsupported ternary arithmetic operation {}", util::to_underlying(n.op()));
    }

    builder_.set_and_allocate_flags(inverse_carry_flag_operation);
}

void arm64_translation_context::materialise_binary_atomic(const binary_atomic_node &n) {
	const auto &destination = vreg_alloc_.allocate(n.val());
    const auto &source = materialise_port(n.rhs());
    const register_operand &address = materialise_port(n.address());

    bool sets_flags = true;
    bool inverse_carry_flag_operation = false;

    // FIXME: correct memory ordering?
    // NOTE: not sure if the proper alternative was used (should a/al/l or
    // nothing be used?)

	switch (n.op()) {
	case binary_atomic_op::add:
        builder_.atomic_add(destination, source, address);
        break;
	case binary_atomic_op::sub:
        builder_.atomic_sub(destination, source, address);
        inverse_carry_flag_operation = true;
        break;
    case binary_atomic_op::xadd:
        builder_.atomic_xadd(destination, destination, source);
        break;
	case binary_atomic_op::bor:
        builder_.atomic_or(destination, source, address);
        builder_.cmp(destination, 0);
        inverse_carry_flag_operation = true;
		break;
	case binary_atomic_op::band:
        // TODO: Not sure if this is correct
        builder_.atomic_and(destination, source, address);
		break;
	case binary_atomic_op::bxor:
        builder_.atomic_eor(destination, source, address);
        builder_.cmp(destination, 0);
        inverse_carry_flag_operation = true;
		break;
    case binary_atomic_op::btc:
        builder_.atomic_clr(destination, source, address);
        sets_flags = false;
		break;
    case binary_atomic_op::bts:
        builder_.atomic_or(destination, source, address);
        sets_flags = false;
		break;
    case binary_atomic_op::xchg:
        // TODO: check if this works
        builder_.atomic_swap(destination, source, address);
        sets_flags = false;
        break;
	default:
		throw backend_exception("unsupported binary atomic operation {}", util::to_underlying(n.op()));
	}

    if (sets_flags) {
        builder_.set_and_allocate_flags(inverse_carry_flag_operation);
    }
}

void arm64_translation_context::materialise_ternary_atomic(const ternary_atomic_node &n) {
    // Destination register only used for storing return code of STXR (a 32-bit value)
    // Since STXR expects a 32-bit register; we directly allocate a 32-bit one
    // NOTE: we're only going to use it afterwards for a comparison and an increment
    const register_operand &destination = vreg_alloc_.allocate(n.val(), n.rhs().type());

    const register_operand &accumulator = materialise_port(n.rhs());
    const register_operand &source = materialise_port(n.top());
    const register_operand &address = materialise_port(n.address());

    // CMPXCHG:
    // dest_reg = mem[addr];
    // if (dest_reg != accumulator) accumulator = dest_reg;
    // else try mem[addr] = source;
    //      if (failed) goto beginning
    // end
    switch (n.op()) {
    case ternary_atomic_op::cmpxchg:
        builder_.atomic_cmpxchg(destination, accumulator, source, address);
        break;
    default:
		throw backend_exception("unsupported binary atomic operation {}", util::to_underlying(n.op()));
    }

    builder_.set_and_allocate_flags(false);
}

void arm64_translation_context::materialise_unary_arith(const unary_arith_node &n) {
    const register_operand &destination = vreg_alloc_.allocate(n.val());
    const register_operand &lhs = materialise_port(n.lhs());

    switch (n.op()) {
    case unary_arith_op::bnot:
        builder_.complement(destination, lhs);
        break;
    case unary_arith_op::neg:
        builder_.negate(destination, lhs);
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
    auto &src_vregs = materialise_port(n.source_value());

    // Allocate as many destination registers as necessary
    // TODO: this is not exactly correct, since we need to create different
    // registers of the base type in such cases
    const auto &dest_vregs = vreg_alloc_.allocate(n.val());

    auto &src_vreg = src_vregs[0];
    const auto &dest_vreg = dest_vregs[0];

    logger.debug("Materializing cast operation: {}\n", n.op());
	switch (n.op()) {
	case cast_op::sx:
        // Sanity check
        if (n.val().type().element_width() <= n.source_value().type().element_width())
            throw backend_exception("cannot sign-extend {} to smaller size {}",
                                    n.val().type().element_width(),
                                    n.source_value().type().element_width());


        // IDEA:
        // 1. Sign-extend reasonably
        // 2. If dest_value > 64-bit, determine sign
        // 3. Plaster sign all over the upper bits
        builder_.insert_comment("sign-extend from {} to {}", n.source_value().type(), n.val().type());
        switch (n.source_value().type().element_width()) {
        case 1:
            // 1 -> N
            // sign-extend to 1 byte
            // sign-extend the rest
            // TODO: this is likely wrong
            builder_.lsl(dest_vreg, src_vreg, 7)
                    .add_comment("shift left LSB to set sign bit of byte");
            builder_.sxtb(dest_vreg, dest_vreg).add_comment("sign-extend");
            builder_.asr(dest_vreg, dest_vreg, 7)
                    .add_comment("shift right to fill LSB with sign bit (except for least-significant bit)");
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
                builder_.asr(dest_vregs[i], dest_vregs[i], 64);
            }
        }
        break;
	case cast_op::bitcast:
        // Simply change the meaning of the bit pattern
        // dest_vreg is set to the desired type already, but it must have the
        // value of src_vreg
        // A simple mov is sufficient (eliminated anyway by the register
        // allocator)
        builder_.insert_comment("Bitcast from {} to {}", n.source_value().type(), n.val().type());

        logger.debug("Bitcasting from {}x{} to {}x{}\n",
                     src_vregs.size(), src_vregs[0].type(),
                     dest_vregs.size(), dest_vregs[0].type());

        if (dest_vregs.size() == src_vregs.size()) {
            if (n.source_value().type().is_floating_point() || n.val().type().is_floating_point()) {
                for (std::size_t i = 0; i < dest_vregs.size(); ++i)
                        builder_.fmov(dest_vregs[i], src_vregs[i]);
                return;
            }

            for (std::size_t i = 0; i < dest_vregs.size(); ++i)
                builder_.mov(dest_vregs[i], src_vregs[i]);
            return;
        }

        if (n.val().type().element_width() > n.source_value().type().element_width()) {
            // Destination consists of fewer elements but of larger widths
            std::size_t dest_idx = 0;
            std::size_t dest_pos = 0;
            for (std::size_t i = 0; i < src_vregs.size(); ++i) {
                builder_.lsl(src_vregs[i], src_vregs[i], dest_pos % n.val().type().element_width());
                builder_.mov(dest_vregs[dest_idx], src_vreg);

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
                builder_.lsl(src_vreg, src_vreg, src_pos % n.source_value().type().element_width());
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
        [[unlikely]]
        if (n.val().type().element_width() <= n.source_value().type().element_width())
            throw backend_exception("Cannot zero-extend {} to smaller size {}",
                                    n.val().type(), n.source_value().type());

        builder_.insert_comment("zero-extend from {} to {}", n.source_value().type(), n.val().type());
        if (n.source_value().type().element_width() <= 8) {
            builder_.uxtb(dest_vreg, src_vreg);
            return;
        }

        if (n.source_value().type().element_width() <= 16) {
            builder_.uxth(dest_vreg, src_vreg);
            return;
        }

        if (n.source_value().type().element_width() <= 32) {
            builder_.uxtw(dest_vreg, src_vreg);
            return;
        }

        if (n.source_value().type().element_width() <= 256) {
            for (std::size_t i = 0; i < src_vregs.size(); ++i) {
                builder_.mov(dest_vregs[i], src_vregs[i]);
            }

            // Set upper registers to zero
            if (dest_vregs.size() > 1) {
                builder_.insert_comment("Set upper registers to zero");
                for (std::size_t i = src_vregs.size(); i < dest_vregs.size(); ++i)
                    builder_.mov(dest_vregs[i], 0);
            }
            return;
        }

        throw backend_exception("Cannot zero-extend from {} to {}", n.source_value().type(), n.val().type());
    case cast_op::trunc:
        [[unlikely]]
        if (dest_vreg.type().element_width() > src_vreg.type().element_width())
            throw backend_exception("Cannot truncate from {} to large size {}",
                                    dest_vreg.type(), src_vreg.type());

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
            builder_.and_(src_vreg, src_vreg, 1);
            builder_.mov(dest_vreg, src_vreg);
        } else if (src_vregs.size() == 1) {
            // TODO: again register reallocation problems, this should be clearly
            // specified as a smaller size
            immediate_operand immediate = 64 - dest_vreg.type().element_width();
            if (src_vreg.type().element_width() > dest_vreg.type().element_width())
                src_vreg.cast(dest_vreg.type());
            builder_.lsl(dest_vreg, src_vreg, immediate);
            builder_.asr(dest_vreg, dest_vreg, immediate);
        }
        break;
    case cast_op::convert:
        // convert between integer and float representations
        [[unlikely]]
        if (dest_vregs.size() != 1)
            throw backend_exception("Cannot convert {} because it is larger than 64-bits",
                                    n.val().type());

        // convert integer to float
        if (n.source_value().type().is_integer() && n.val().type().is_floating_point()) {
             if (n.val().type().type_class() == value_type_class::unsigned_integer)
                builder_.ucvtf(dest_vreg, src_vreg);
             else
                builder_.scvtf(dest_vreg, src_vreg);
        } else if (n.source_value().type().is_floating_point() && n.val().type().is_integer()) {
            // Handle float/double -> integer conversions
            switch (n.convert_type()) {
            case fp_convert_type::trunc:
                // if float/double -> truncate to int
                // NOTE: both float/double handled through the same instructions,
                // only register differ
                if (n.val().type().type_class() == value_type_class::unsigned_integer)
                    builder_.fcvtzu(dest_vreg, src_vreg);
                else
                    builder_.fcvtzs(dest_vreg, src_vreg);
                break;
            case fp_convert_type::round:
                // if float/double -> round to closest int
                // NOTE: both float/double handled through the same instructions,
                // only register differ
                if (n.val().type().type_class() == value_type_class::unsigned_integer)
                    builder_.fcvtau(dest_vreg, src_vreg);
                else
                    builder_.fcvtas(dest_vreg, src_vreg);
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
    [[unlikely]]
    if (n.val().type().is_vector() || n.val().type().element_width() > value_types::base_type.element_width())
        throw backend_exception("Cannot implement conditional selection for type {}", n.val().type());

    const auto &dest = vreg_alloc_.allocate(n.val());
    const auto &condition = materialise_port(n.condition());
    const auto &true_var = materialise_port(n.trueval());
    const auto &false_var = materialise_port(n.falseval());

    builder_.cmp(condition, 0)
            .add_comment("compare condition for conditional select");
    builder_.csel(dest, true_var, false_var, cond_operand::ne());
}

void arm64_translation_context::materialise_bit_shift(const bit_shift_node &n) {
    // Generally, cannot implement them for vectors or > 64-bit values
    [[unlikely]]
    if (n.val().type().is_vector())
        throw backend_exception("Cannot implement {} for type {}", n.op(), n.val().type());

    [[unlikely]]
    if (n.amount().type().is_vector() || n.amount().type().element_width() > value_types::base_type.element_width())
        throw backend_exception("Cannot {} by amount type {}", n.op(), n.val().type());

    // TODO: refactor this
    const auto& input = materialise_port(n.input());

    auto& amount = materialise_port(n.amount());
    amount = builder_.cast(amount, n.val().type());

    const auto& dest_vreg = vreg_alloc_.allocate(n.val());

    switch (n.op()) {
    case shift_op::lsl:
        switch (n.val().type().element_width()) {
        case 1:
        case 8:
        case 16:
        case 32:
        case 64:
            builder_.lsl(dest_vreg, input, amount);
            break;
        default:
            throw backend_exception("Unsupported logical left-shift operation");
        }
        break;
    case shift_op::lsr:
        switch (n.val().type().element_width()) {
        case 1:
        case 8:
        case 16:
        case 32:
        case 64:
            builder_.lsr(dest_vreg, input, amount);
            break;
        case 128:
            {
                [[unlikely]]
                if (n.amount().owner()->kind() != node_kinds::constant)
                    throw backend_exception("Cannot generate logical right-shift for 128-bit with non-constant shift amount");
                auto amount_imm = reinterpret_cast<const constant_node*>(n.amount().owner())->const_val_i();
                if (amount_imm < 64) {
                    builder_.extr(dest_vreg[0], input[1], input[0], amount_imm);
                    builder_.lsr(dest_vreg[1], input[1], amount);
                } else if (amount_imm == 64) {
                    builder_.mov(dest_vreg[0], input[1]);
                    builder_.mov(dest_vreg[1], 0);
                } else {
                    throw backend_exception("Unsupported logical right-shift operation with amount {}", amount_imm);
                }
            }
            break;
        default:
            throw backend_exception("Unsupported logical right-shift operation");
        }
        break;
    case shift_op::asr:
        switch (n.val().type().element_width()) {
        case 1:
        case 8:
        case 16:
        case 32:
        case 64:
            builder_.asr(dest_vreg, input, amount);
            break;
        default:
            throw backend_exception("Unsupported arithmetic right-shift operation");
        }
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

    auto dest_total_width = n.val().type().width();
    auto reg_extract_start = n.from() / src_vregs[0].type().element_width();

    std::size_t extracted = 0;
    auto reg_extract_idx = n.from() % src_vregs[0].type().element_width();
    auto extract_len = std::min(src_vregs[0].type().element_width() - reg_extract_idx, n.length());

    std::size_t dest_idx = 0;

    for (std::size_t i = 0; i < dest_vregs.size(); ++i)
        builder_.mov(dest_vregs[i], 0);

    builder_.insert_comment("Extract specific bits into destination");
    for (std::size_t i = reg_extract_start; extracted < n.length(); ++i) {
        if (reg_extract_idx == 0 && reg_extract_idx + extract_len == dest_vregs[dest_idx].type().element_width()) {
            builder_.mov(dest_vregs[dest_idx], src_vregs[i]);
            reg_extract_idx = 0;
            extracted += extract_len;
            extract_len = std::min(n.length() - extracted, src_vregs[i].type().element_width());
            dest_idx = extracted % dest_total_width;
            continue;
        }

        dest_vregs[dest_idx].cast(src_vregs[i].type());
        builder_.ubfx(dest_vregs[dest_idx], src_vregs[i], reg_extract_idx, extract_len);
        reg_extract_idx = 0;
        extracted += extract_len;
        extract_len = std::min(n.length() - extracted, src_vregs[i].type().element_width());
        dest_idx = extracted % dest_total_width;
    }
}

void arm64_translation_context::materialise_bit_insert(const bit_insert_node &n) {
    auto &insertion_bits = materialise_port(n.bits());
    const auto &src  = materialise_port(n.source_value());
    const auto &dest = vreg_alloc_.allocate(n.val());

    // Sanity check
    [[unlikely]]
    if (dest.size() != src.size())
        throw backend_exception("Source and destination mismatch for bit insert node (dest: {} != src: {}",
                                dest.size(), src.size());

    // Copy source to dest
    for (std::size_t i = 0; i < dest.size(); ++i) {
        builder_.mov(dest[i], src[i]).as_keep()
                    .add_comment("Copy source to destination (insertion will overwrite)");
    }

    // Algorithm:
    // Need to insert into either one or multiple registers
    // Situations handled separately
    std::size_t element_width = dest[0].type().element_width();

    std::size_t insert_idx = n.to() % element_width;
    std::size_t insert_len = std::min(element_width - insert_idx, n.length());

    [[unlikely]]
    if (insert_len == 0)
        throw backend_exception("Cannot insert into invalid range [{}:{})", n.to(), n.to()+insert_len);

    builder_.insert_comment("insert specific bits into [{}:{}) with destination of type {}",
                             n.to(), n.to()+insert_len, n.val().type());

    [[likely]]
    if (dest.size() == 1) {
        auto out = builder_.cast(insertion_bits, dest[0].type());
        builder_.bfi(dest, out, insert_idx, insert_len);
        return;
    }

    std::size_t bits_idx = 0;
    std::size_t bits_total_width = total_width(insertion_bits);

    std::size_t inserted = 0;
    std::size_t insert_start = n.to() / dest[0].type().element_width();
    for (std::size_t i = insert_start; inserted < n.length(); ++i) {
        auto out = builder_.cast(insertion_bits[bits_idx], dest[i].type());

        builder_.bfi(dest[i], out, insert_idx, insert_len);
        insert_idx = 0;
        inserted += insert_len;

        insert_len = std::min(n.length() - inserted, insertion_bits[bits_idx].type().element_width());
        if (insert_len == 0)
            return;

        bits_idx = inserted % bits_total_width;
    }
}

void arm64_translation_context::materialise_vector_insert(const vector_insert_node &n) {
    const auto &dest_vregs = vreg_alloc_.allocate(n.val());
    const auto &src_vregs = materialise_port(n.source_vector());
    const auto &value_vregs = materialise_port(n.insert_value());

    [[unlikely]]
    if (dest_vregs.size() < src_vregs.size())
        throw backend_exception("Destination vector for vector insert is smaller than source vector");

    [[unlikely]]
    if (dest_vregs.size() == 0 || src_vregs.size() == 0 || value_vregs.size() == 0)
        throw backend_exception("Cannot perform vector insertion with 0-size registers");

    std::size_t index = (n.index() * n.val().type().element_width()) / dest_vregs[0].type().element_width();

    [[unlikely]]
    if (index + value_vregs.size() > dest_vregs.size())
        throw backend_exception("Cannot insert at index {} in destination vector", index);

    builder_.insert_comment("Insert vector by first copying source to destination");
    for (std::size_t i = 0; i < src_vregs.size(); ++i)
        builder_.mov(dest_vregs[i], src_vregs[i]);

    builder_.insert_comment("Insert value of type {} into destination at index {}",
                            n.insert_value().type(), n.index());
    for (std::size_t i = 0; i < value_vregs.size(); ++i) {
        const auto &value_vreg = builder_.cast(value_vregs[i], dest_vregs[index + i].type());
        builder_.mov(dest_vregs[index + i], value_vreg);
    }
}

void arm64_translation_context::materialise_vector_extract(const vector_extract_node &n) {
    auto &dest_vregs = vreg_alloc_.allocate(n.val());
    const auto &src_vregs = materialise_port(n.source_vector());

    std::size_t index = (n.index() * n.source_vector().type().element_width()) / value_types::base_type.element_width();
    if (dest_vregs.size() >= src_vregs.size())
        throw backend_exception("Cannot extract vector larger than source vector");
    if (index + dest_vregs.size() > src_vregs.size())
        throw backend_exception("Cannot extract from index {} in source vector", index);

    builder_.insert_comment("Extract vector by copying to destination");
    if (n.val().type().is_floating_point()) {
        for (std::size_t i = 0; i < dest_vregs.size(); ++i) {
            // TODO: wrong
            dest_vregs[i].cast(src_vregs[index+i].type());
            builder_.fmov(dest_vregs[i], src_vregs[index+i]);
        }
        return;
    }

    for (std::size_t i = 0; i < dest_vregs.size(); ++i) {
        const auto &src_vreg = builder_.cast(src_vregs[index+i], dest_vregs[i].type());
        builder_.mov(dest_vregs[i], src_vreg);
    }
}

void arm64_translation_context::materialise_internal_call(const internal_call_node &n) {
    if (n.fn().name() == "handle_syscall") {
        ret_ = 1;
    } else if (n.fn().name() == "handle_int") {
        ret_ = 2;
    } else if (n.fn().name() == "hlt") {
        ret_ = 2;
    } else {
        throw backend_exception("unsupported internal call: {}", n.fn().name());
    }
}

void arm64_translation_context::materialise_read_local(const read_local_node &n) {
    [[unlikely]]
    if (locals_.count(n.local()) == 0)
        throw backend_exception("Attempting to read local@{} of type {} that does not exist",
                                fmt::ptr(n.local()), n.local()->type());

    const auto &dest_vregs = vreg_alloc_.allocate(n.val());
    const auto &locals = locals_[n.local()];

    [[unlikely]]
    if (locals.size() != dest_vregs.size())
        throw backend_exception("Read local received mismatched types for locals {}" \
                                "(register count {}) and destination {} (register count {})",
                                 n.local()->type(), locals.size(), n.val().type(), dest_vregs.size());

    builder_.insert_comment("Read local variable @{}", fmt::ptr(n.local()));
    for (std::size_t i = 0; i < dest_vregs.size(); ++i)
        builder_.mov(dest_vregs[i], locals[i]);
}

void arm64_translation_context::materialise_write_local(const write_local_node &n) {
    const auto &write_regs = materialise_port(n.write_value());
    if (locals_.count(n.local()) == 0) {
        const auto& dest_vregs = vreg_alloc_.allocate(n.write_value().type());
        locals_.emplace(n.local(), dest_vregs);
    }

    builder_.insert_comment("Write local variable @{} with register {}", fmt::ptr(n.local()), write_regs[0]);
    const auto &dest_vregs = locals_[n.local()];

    [[unlikely]]
    if (write_regs.size() != dest_vregs.size())
        throw backend_exception("Write local received mismatched types: {} (register count {}) and {} (register count {})",
                                 n.write_value().type(), write_regs.size(), n.val().type(), dest_vregs.size());


    for (std::size_t i = 0; i < dest_vregs.size(); ++i)
        builder_.mov(dest_vregs[i], write_regs[i]);
}

