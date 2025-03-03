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

using arancini::input::x86::reg_offsets;

memory_operand arm64_translation_context::guest_memory(int regoff, memory_operand::address_mode mode) {
    if (regoff > 255 || regoff < -256) {
        const auto& base = vreg_alloc_.allocate(value_types::addr_type);
        builder_.move_to_variable(base, regoff);
        builder_.add(base, base, scalar(builder_.context_block()));
        return memory_operand(base.as_scalar(), 0, mode);
    } else {
        return memory_operand(builder_.context_block(), regoff, mode);
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

        builder_.insert_comment("=====================================================");
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

        builder_.ret(ret_);

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

void arm64_translation_context::materialise_read_reg(const read_reg_node &n) {
    const auto& destination = vreg_alloc_.get_or_allocate(n.val());
    builder_.insert_comment("read register: {}", n.regname());
    builder_.load(destination, guest_memory(n.regoff()));
}

static inline bool is_flag_port(const port &value) {
	return value.type().width() == 1 || value.kind() == port_kinds::zero ||
           value.kind() == port_kinds::carry || value.kind() == port_kinds::negative ||
           value.kind() == port_kinds::overflow;
}

inline bool is_flag_setter(node_kinds node_kind) {
    return node_kind == node_kinds::binary_arith || node_kind == node_kinds::ternary_arith ||
           node_kind == node_kinds::binary_atomic || node_kind == node_kinds::ternary_atomic;
}

void arm64_translation_context::materialise_write_reg(const write_reg_node &n) {
    const auto &source = materialise_port(n.value());

    // Flags may be set either based on some preceding operation or with a constant
    // Handle the case when they are generated based on a previous operation here
    if (is_flag_port(n.value())) {
        builder_.insert_comment("write flag: {}", n.regname());
        if (is_flag_setter(n.value().owner()->kind())) {
            const auto &flag = builder_.flag(static_cast<reg_offsets>(n.regoff()));
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
    const auto& destination = vreg_alloc_.get_or_allocate(n.val());
    const auto &address = materialise_port(n.address());
    builder_.load(destination, memory_operand(address.as_scalar()));
}

void arm64_translation_context::materialise_write_mem(const write_mem_node &n) {
    const auto &src = materialise_port(n.value());
    const auto &address = materialise_port(n.address());
    builder_.store(src, memory_operand(address.as_scalar()));
}

void arm64_translation_context::materialise_read_pc(const read_pc_node &n) {
	auto destination = vreg_alloc_.allocate(n.val());
    builder_.insert_comment("read program counter");
    builder_.move_to_variable(destination, this_pc_);
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
    builder_.branch(label);
}

void arm64_translation_context::materialise_cond_br(const cond_br_node &n) {
    const auto &condition = materialise_port(n.cond());

    auto label_name = fmt::format("{}_{}", n.target()->name(), instr_cnt_);
    logger.debug("Generating conditional branch to label {}\n", label_name);
    auto label = label_operand(label_name);
    builder_.zero_compare_and_branch(condition.as_scalar(), label, cond_operand::ne());
}

void arm64_translation_context::materialise_constant(const constant_node &n) {
	const auto &destination = vreg_alloc_.allocate(n.val());

    [[unlikely]]
    if (n.val().type().is_floating_point())
        builder_.move_to_variable(destination, n.const_val_f());
    else
        builder_.move_to_variable(destination, n.const_val_i());
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
	const auto &destination = vreg_alloc_.allocate(n.val());

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
        {
            auto type = ir::value_type(lhs.type().type_class(), lhs.type().width() / 2);
            auto lhs_src = builder_.truncate(lhs, type);
            auto rhs_src = builder_.truncate(rhs, type);
            builder_.multiply(destination, lhs_src, rhs_src);
        }
        sets_flags = false;
        break;
	case binary_arith_op::div:
        {
            auto type = ir::value_type(lhs.type().type_class(), lhs.type().width() / 2);
            auto lhs_src = builder_.truncate(lhs, type);
            auto rhs_src = builder_.truncate(rhs, type);
            destination.type() = ir::value_type(destination.type().type_class(),
                                                destination.type().element_width() / 2);
            builder_.divide(destination, lhs, rhs);
        }
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
            auto type = ir::value_type(lhs.type().type_class(), lhs.type().width() / 2);
            auto lhs_src = builder_.truncate(lhs, type);
            auto rhs_src = builder_.truncate(rhs, type);
            auto temp1 = vreg_alloc_.allocate(type);
            auto temp2 = vreg_alloc_.allocate(destination.type());
            builder_.divide(temp1, lhs_src, rhs_src);
            builder_.multiply(temp2, rhs_src, temp1);
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
        builder_.subs(destination, lhs, rhs);
        builder_.complement(destination, destination);
        break;
	case binary_arith_op::cmpne:
        builder_.subs(destination,lhs, rhs);
        break;
	case binary_arith_op::cmpgt:
        builder_.subs(destination,lhs, rhs);
        builder_.conditional_set(destination, get_cset_type(n.op()));
        break;
	case binary_arith_op::cmpoeq:
	case binary_arith_op::cmpolt:
	case binary_arith_op::cmpole:
	case binary_arith_op::cmpo:
	case binary_arith_op::cmpu:
        builder_.comparison(lhs, rhs);
        builder_.conditional_set(destination, get_cset_type(n.op()));
        break;
	case binary_arith_op::cmpueq:
	case binary_arith_op::cmpune:
	case binary_arith_op::cmpult:
	case binary_arith_op::cmpunlt:
	case binary_arith_op::cmpunle:
        {
            builder_.comparison(lhs, rhs);
            const auto& unordered = vreg_alloc_.allocate(value_type::u64());
            builder_.conditional_set(destination, get_cset_type(n.op()));
            builder_.conditional_set(unordered, cond_operand::vs());
            builder_.logical_or(destination, destination, unordered);
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
    const auto& lhs = materialise_port(n.lhs()).as_scalar();
    const auto& rhs = materialise_port(n.rhs()).as_scalar();
    const auto& top = materialise_port(n.top()).as_scalar();
	const auto& destination = vreg_alloc_.allocate(n.val()).as_scalar();

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
	const auto &destination = vreg_alloc_.allocate(n.val()).as_scalar();
    const auto &source = materialise_port(n.rhs()).as_scalar();
    const auto &address = materialise_port(n.address()).as_scalar();

    bool sets_flags = true;
    bool inverse_carry_flag_operation = false;

	switch (n.op()) {
	case binary_atomic_op::add:
        builder_.atomic_add(destination, source, address);
        break;
	case binary_atomic_op::sub:
        builder_.atomic_sub(destination, source, address);
        inverse_carry_flag_operation = true;
        break;
    case binary_atomic_op::xadd:
        builder_.atomic_xadd(destination, source, address);
        break;
	case binary_atomic_op::bor:
        builder_.atomic_or(destination, source, address);
        builder_.comparison(destination, 0);
        inverse_carry_flag_operation = true;
		break;
	case binary_atomic_op::band:
        // TODO: Not sure if this is correct
        builder_.atomic_and(destination, source, address);
		break;
	case binary_atomic_op::bxor:
        builder_.atomic_eor(destination, source, address);
        builder_.comparison(destination, 0);
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
    const scalar &destination = vreg_alloc_.allocate(n.val(), n.rhs().type());
    const scalar &accumulator = materialise_port(n.rhs());
    const scalar &source = materialise_port(n.top());
    const scalar &address = materialise_port(n.address());

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
    const scalar &destination = vreg_alloc_.allocate(n.val());
    const scalar &lhs = materialise_port(n.lhs());

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
    const auto &source = materialise_port(n.source_value());
    const auto &destination = vreg_alloc_.allocate(n.val());

    logger.debug("Materializing cast operation: {}\n", n.op());
    switch (n.op()) {
    case cast_op::sx:
        builder_.sign_extend(destination, source);
        break;
    case cast_op::zx:
        builder_.zero_extend(destination, source);
        break;
    case cast_op::bitcast:
        builder_.bitcast(destination, source);
        break;
    case cast_op::convert:
        builder_.convert(destination, source, n.convert_type());
        break;
    case cast_op::trunc:
        builder_.truncate(destination, source);
        break;
    default:
        throw backend_exception("Cannot handle cast with index: {}",
                                util::to_underlying(n.op()));
    }
}

void arm64_translation_context::materialise_csel(const csel_node &n) {
    const scalar &dest = vreg_alloc_.allocate(n.val());
    const scalar &condition = materialise_port(n.condition());
    const scalar &true_var = materialise_port(n.trueval());
    const scalar &false_var = materialise_port(n.falseval());

    builder_.comparison(condition, 0)
            .add_comment("compare condition for conditional select");
    builder_.conditional_select(dest, true_var, false_var, cond_operand::ne());
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
    const auto& amount = materialise_port(n.amount());
    const auto& destination = vreg_alloc_.allocate(n.val());

    switch (n.op()) {
    case shift_op::lsl:
        builder_.left_shift(destination, input, amount);
        break;
    case shift_op::lsr:
        builder_.logical_right_shift(destination, input, amount);
        break;
    case shift_op::asr:
        builder_.arithmetic_right_shift(destination, input, amount);
        break;
    default:
        throw backend_exception("Unsupported shift operation: {}", n.op());
    }
}

void arm64_translation_context::materialise_bit_extract(const bit_extract_node &n) {
    const auto &source = materialise_port(n.source_value());
    const auto &destination = vreg_alloc_.allocate(n.val());

    builder_.bit_extract(destination, source, n.from(), n.length());
}

void arm64_translation_context::materialise_bit_insert(const bit_insert_node &n) {
    const auto &destination = vreg_alloc_.allocate(n.val());
    const auto &source  = materialise_port(n.source_value());
    const auto &insertion_bits = materialise_port(n.bits());
    builder_.insert_comment("insert specific bits into [{}:{}) with destination of type {}",
                             n.to(), n.to()+n.length(), n.val().type());
    builder_.bit_insert(destination, source, insertion_bits, n.to(), n.length());
}

void arm64_translation_context::materialise_vector_insert(const vector_insert_node &n) {
    const auto &destination = vreg_alloc_.allocate(n.val());
    const auto &source = materialise_port(n.source_vector());
    const auto &insertion_bits = materialise_port(n.insert_value());

    builder_.insert_comment("Insert value of type {} into destination at index {}",
                            n.insert_value().type(), n.index());

    auto index = n.index() * destination.type().element_width();
    builder_.bit_insert(destination, source, insertion_bits, index, insertion_bits.type().width());
}

void arm64_translation_context::materialise_vector_extract(const vector_extract_node &n) {
    auto &destination = vreg_alloc_.allocate(n.val());
    const auto &source = materialise_port(n.source_vector());
    builder_.bit_extract(destination, source, n.index(), destination.type().width());
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

    const auto &destination = vreg_alloc_.allocate(n.val());
    const auto &local_variable = locals_[n.local()];

    builder_.insert_comment("Read local variable @{}", fmt::ptr(n.local()));
    builder_.move_to_variable(destination, local_variable);
}

void arm64_translation_context::materialise_write_local(const write_local_node &n) {
    const auto &source = materialise_port(n.write_value());
    if (locals_.count(n.local()) == 0) {
        const auto& destination = vreg_alloc_.allocate(n.write_value().type());
        locals_.emplace(n.local(), destination);
    }

    const auto &local_variable = locals_[n.local()];

    builder_.insert_comment("Write local variable @{}", fmt::ptr(n.local()));
    builder_.move_to_variable(local_variable, source);
}

