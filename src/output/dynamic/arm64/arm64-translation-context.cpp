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

// TODO: should be replaced with static_map data structure
using flag_map_type = std::unordered_map<reg_offsets, register_operand>;
static flag_map_type flag_map {
	{ reg_offsets::ZF, {} },
	{ reg_offsets::CF, {} },
	{ reg_offsets::OF, {} },
	{ reg_offsets::SF, {} },
};

template <typename NodeType>
void allocate_flags(port_register_allocator& allocator, flag_map_type& flag_map, const NodeType& n) {
    flag_map[reg_offsets::ZF] = allocator.allocate(n.zero(), value_type::u1());
    flag_map[reg_offsets::SF] = allocator.allocate(n.negative(), value_type::u1());
    flag_map[reg_offsets::OF] = allocator.allocate(n.overflow(), value_type::u1());
    flag_map[reg_offsets::CF] = allocator.allocate(n.carry(), value_type::u1());
}

void fill_byte_with_bit(instruction_builder& builder, const register_operand& reg) {
    builder.lsl(reg, reg, 7)
                .add_comment("shift left LSB to set sign bit of byte");
    builder.asr(reg, reg, 7)
                .add_comment("shift right to fill LSB with sign bit (except for least-significant bit)");
}

register_operand arm64_translation_context::cast(const register_operand &src, value_type type) {
    builder_.insert_comment("Internal cast from {} to {}", src.type(), type);

	if (src.type().type_class() == value_type_class::floating_point &&
        type.type_class() != value_type_class::floating_point) {
		auto dest = vreg_alloc_.allocate(type);
        builder_.fcvtzs(dest, src);
        return dest;
	}

	if (src.type().type_class() == value_type_class::floating_point &&
        type.type_class() == value_type_class::floating_point) {
        if (type.element_width() == 64 && src.type().element_width() == 32) {
            auto dest = vreg_alloc_.allocate(type);
            builder_.fcvt(dest, src);
            return dest;
        }

        if (type.element_width() == 64 && src.type().element_width() == 64)
            return src;
        
        if (type.element_width() == 64 && src.type().element_width() == 128) {
            auto dest = vreg_alloc_.allocate(src.type());
            builder_.fmov(dest, src);
            dest[0].cast(type);
            return dest;
        }

        throw backend_exception("Cannot internally cast from {} to {}", src.type(), type);
    }

    if (src.type().element_width() >= type.element_width()) {
        return register_operand(src.index(), type);
    }

    if (type.element_width() > 64)
        type = value_type::u64();

    auto dest = vreg_alloc_.allocate(type);
    builder_.sign_extend(dest, src);
    return dest;
}

memory_operand arm64_translation_context::guest_memory(int regoff, memory_operand::address_mode mode) {
    if (regoff > 255 || regoff < -256) {
        register_operand base_vreg = vreg_alloc_.allocate(value_types::addr_type);
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

    [[unlikely]]
    if (type.is_vector() && type.element_width() > value_types::base_type.element_width())
        throw backend_exception("Cannot load registers of type {}", type);

    auto comment = fmt::format("read register: {}", n.regname());
    auto& dest_vregs = vreg_alloc_.allocate(n.val());
    auto address = guest_memory(n.regoff());
    builder_.load(variable(dest_vregs), address);
}

inline bool is_flag_setter(node_kinds node_kind) {
    return node_kind == node_kinds::binary_arith || node_kind == node_kinds::ternary_arith ||
           node_kind == node_kinds::binary_atomic || node_kind == node_kinds::ternary_atomic;
}

void arm64_translation_context::materialise_write_reg(const write_reg_node &n) {
    // Sanity check
    auto type = n.val().type();
    auto &src = materialise_port(n.value());

    [[unlikely]]
    if (type.is_vector() && type.element_width() > value_types::base_type.element_width())
        throw backend_exception("Cannot store registers of type {}", type);

    // Flags may be set either based on some preceding operation or with a constant
    // Handle the case when they are generated based on a previous operation here
    if (is_flag_port(n.value())) {
        if (is_flag_setter(n.value().owner()->kind())) {
            const auto &source = flag_map.at(static_cast<reg_offsets>(n.regoff()));
            builder_.append(arm64_assembler::strb(source, guest_memory(n.regoff())))
                         .add_comment(fmt::format("write flag: {}", n.regname()));
        } else if (src.size()) {
            src[0].cast(n.value().type());
            builder_.append(arm64_assembler::strb(src[0], guest_memory(n.regoff())))
                         .add_comment(fmt::format("write flag: {}", n.regname()));
        }
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
    auto comment = fmt::format("write register: {}", n.regname());
    auto address = guest_memory(n.regoff());
    builder_.store(variable(src), address);
}

void arm64_translation_context::materialise_read_mem(const read_mem_node &n) {
    // Sanity checks
    auto type = n.val().type();

    [[unlikely]]
    if (type.is_vector() && type.element_width() > value_types::base_type.element_width())
        throw backend_exception("Cannot load vectors from memory with type {}", type);

    const auto &dest = vreg_alloc_.allocate(n.val());
    const register_operand &address = materialise_port(n.address());
    builder_.load(variable(dest), address);
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
    builder_.store(variable(src), address);
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

    builder_.append(arm64_assembler::str(new_pc_vreg, guest_memory(reg_offsets::PC)))
            .add_comment("write program counter");
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
    const auto &cond_vregs = materialise_port(n.cond());

    auto label_name = fmt::format("{}_{}", n.target()->name(), instr_cnt_);
    logger.debug("Generating conditional branch to label {}\n", label_name);
    auto label = label_operand(label_name);
    builder_.zero_compare_and_branch(cond_vregs, label, cond_operand::ne());
}

void arm64_translation_context::materialise_constant(const constant_node &n) {
	const auto &dest = vreg_alloc_.allocate(n.val());

    [[unlikely]]
    if (n.val().type().is_floating_point())
        builder_.fmov(dest, n.const_val_f()).add_comment("move float into register");
    else
        builder_.mov(dest, n.const_val_i()).add_comment("move integer into register");
}

inline shift_operand extend_register(instruction_builder& builder, const register_operand& reg, arancini::ir::value_type type) {
    auto mod = shift_operand::shift_type::lsl;

    switch (type.element_width()) {
    case 8:
        if (type.type_class() == value_type_class::signed_integer) {
            mod = shift_operand::shift_type::sxtb;
        } else {
            mod = shift_operand::shift_type::uxtb;
        }
        break;
    case 16:
        if (type.type_class() == value_type_class::signed_integer) {
            mod = shift_operand::shift_type::sxth;
        } else {
            mod = shift_operand::shift_type::uxth;
        }
        break;
    }
    builder.extend(reg, reg);

    return shift_operand(mod, 0);
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
    auto &lhs_regset = materialise_port(n.lhs());
    auto &rhs_regset = materialise_port(n.rhs());
	auto &dest_regset = vreg_alloc_.allocate(n.val());

    // Sanity check
    // Binary operations are defined in the IR with same size inputs and output
    [[unlikely]]
    if (n.lhs().type() != n.rhs().type() || n.lhs().type() != n.val().type()) {
        throw backend_exception("Binary operations not supported between types {} = {} op {}",
                                n.val().type(), n.lhs().type(), n.rhs().type());
    }

    [[unlikely]]
    if (lhs_regset.size() != rhs_regset.size() || lhs_regset.size() != dest_regset.size()) {
        throw backend_exception("Binary operations not supported between types {} = {} op {}",
                                n.val().type(), n.lhs().type(), n.rhs().type());
    }

    bool sets_flags = true;
    bool inverse_carry_flag_operation = false;
    const bool is_vector_op = n.val().type().is_vector();

    // TODO: Somehow avoid allocating this
    allocate_flags(vreg_alloc_, flag_map, n);

    auto mul_impl = [&](const register_sequence& dest_regset,
                        register_sequence& lhs_regset,
                        register_sequence& rhs_regset)
    {
        // Vector multiplication
        // TODO: replace by efficient vectorized version
        if (n.val().type().is_vector()) {
            sets_flags = false;
            for (std::size_t i = 0; i < dest_regset.size(); ++i)
                builder_.mul(dest_regset[i], lhs_regset[i], rhs_regset[i]);
            return;
        }

        // The input and the output have the same size:
        // For 32-bit multiplication: 64-bit output and signed-extended 32-bit values to 64-bit inputs
        // For 64-bit multiplication: 64-bit output and signed-extended 64-bit values to 128-bit inputs
        // NOTE: this is very unfortunate
        switch (n.val().type().element_width()) {
        case 64: // this must perform 32-bit multiplication actually
            // Cast LHS and RHS to 32-bits
            // NOTE: this is guaranteed to yield the same value because we're doing 32-bit
            //       multiplication
            lhs_regset[0].cast(ir::value_type(lhs_regset[0].type().type_class(), 32, 1));
            rhs_regset[0].cast(ir::value_type(rhs_regset[0].type().type_class(), 32, 1));

            switch (n.val().type().type_class()) {
            case ir::value_type_class::signed_integer:
                builder_.smull(dest_regset, lhs_regset, rhs_regset);
                break;
            case ir::value_type_class::unsigned_integer:
                builder_.umull(dest_regset, lhs_regset, rhs_regset);
                break;
            case ir::value_type_class::floating_point:
                // The same fmul is used in both 32-bit and 64-bit multiplication
                // The actual operation depends on the type of its registers
                {
                    auto lhs = cast(lhs_regset, dest_regset[0].type());
                    auto rhs = cast(rhs_regset, dest_regset[0].type());
                    builder_.fmul(dest_regset, lhs, rhs);
                    sets_flags = false;
                }
                break;
            default:
                throw backend_exception("Encounted unknown type class {} for multiplication",
                                        util::to_underlying(n.val().type().type_class()));
            }
            // TODO: need to compute CF and OF
            // CF and OF are set to 1 when lhs * rhs > 64-bits
            // Otherwise they are set to 0
            if (sets_flags) {
                auto compare_regset = vreg_alloc_.allocate(dest_regset[0].type());
                builder_.mov(compare_regset, 0xFFFF0000);
                builder_.compare(variable(compare_regset), variable(dest_regset));
                builder_.cset(flag_map[reg_offsets::CF], cond_operand::ne()).add_comment("compute flag: CF");
                builder_.cset(flag_map[reg_offsets::OF], cond_operand::ne()).add_comment("compute flag: OF");
                sets_flags = false;
            }
            break;
        case 128: // this must perform 64-bit multiplication
            // Integers handled differently than floats
            [[likely]]
            if (n.val().type().type_class() != ir::value_type_class::floating_point) {
                // Get lower 64 bits
                builder_.mul(dest_regset[0], lhs_regset[0], rhs_regset[0]);

                // Get upper 64 bits
                switch (n.val().type().type_class()) {
                case ir::value_type_class::signed_integer:
                    builder_.smulh(dest_regset[1], lhs_regset[0], rhs_regset[0]);
                    break;
                case ir::value_type_class::unsigned_integer:
                    builder_.umulh(dest_regset[1], lhs_regset[0], rhs_regset[0]);
                    break;
                default:
                    throw backend_exception("Encounted unknown type class {} for multiplication",
                                            util::to_underlying(n.val().type().type_class()));
                }
                // TODO: need to compute CF and OF
                // CF and OF are set to 1 when lhs * rhs > 64-bits
                // Otherwise they are set to 0
                builder_.compare(variable(dest_regset[1]), 0);
                builder_.cset(flag_map[reg_offsets::CF], cond_operand::ne()).add_comment("compute flag: CF");
                builder_.cset(flag_map[reg_offsets::OF], cond_operand::ne()).add_comment("compute flag: OF");
                sets_flags = false;
                break;
            } else {
                // TODO: this is incorrect; the entire register set should be in dest_regset
                // Register allocation must then map this accordingly
                // builder_.fmul(dest_regset[0], lhs_regset[0], rhs_regset[0]);
                throw backend_exception("Float multiplication not handled");
                break;
            }
            break;
        default:
            throw backend_exception("Multiplication not supported between {} x {}",
                                    n.lhs().type(), n.rhs().type());
        }
        return;
    };

    auto div_impl = [&](const register_sequence& dest_regset,
                        const register_sequence& lhs_regset,
                        const register_sequence& rhs_regset)
    {
        // Vector division
        // TODO: replace this by efficient vectorized version
        if (n.val().type().is_vector()) {
            sets_flags = false;
            if (n.val().type().type_class() == ir::value_type_class::signed_integer) {
                for (std::size_t i = 0; i < dest_regset.size(); ++i)
                    builder_.sdiv(dest_regset[i], lhs_regset[i], rhs_regset[i]);
            } else if (n.val().type().type_class() == ir::value_type_class::unsigned_integer) {
                for (std::size_t i = 0; i < dest_regset.size(); ++i)
                    builder_.sdiv(dest_regset[i], lhs_regset[i], rhs_regset[i]);
            } else {
                // TODO: support it
                throw backend_exception("Vector division for floating point numbers not supported");
            }
            return;
        }

        // The input and the output have the same size:
        // For 64-bit division: 64-bit input dividend/divisor and 64-bit output but 32-bit division
        // For 128-bit multiplication: 128-bit input dividend/divisor and 128-bit output but 64-bit division
        // NOTE: this is very unfortunate
        // NOTE: we'll need to handle separetely floats
        switch (n.val().type().element_width()) {
        case 64: // this must perform 32-bit division
        case 128: // this must perform 64-bit division
            switch (n.val().type().type_class()) {
            case ir::value_type_class::signed_integer:
                builder_.sdiv(dest_regset[0], lhs_regset[0], rhs_regset[0]);
                break;
            case ir::value_type_class::unsigned_integer:
                builder_.udiv(dest_regset[0], lhs_regset[0], rhs_regset[0]);
                break;
            case ir::value_type_class::floating_point:
                builder_.fdiv(dest_regset[0], lhs_regset[0], rhs_regset[0]);
                break;
            default:
                throw backend_exception("Encounted unknown type class {} for division",
                                        util::to_underlying(n.val().type().type_class()));
            }
            // SDIV and UDIV do not affect the condition flags
            // However, div does not set condition flags for the guest either
            // So we don't need to generate them
            sets_flags = false;
            break;
        default:
            throw backend_exception("Multiplication not supported between {} x {}",
                                    n.lhs().type(), n.rhs().type());
        }
		return;
    };

    value_type op_type;
    if (n.val().type().is_floating_point())
        op_type = n.val().type();
    else
        op_type = value_type(value_type_class::signed_integer, n.val().type().element_width(), n.val().type().nr_elements());

    logger.debug("Binary arithmetic node of type {}\n", n.op());
	switch (n.op()) {
	case binary_arith_op::add:
        // Vector addition
        if (is_vector_op) {
            sets_flags = false;
            for (std::size_t i = 0; i < dest_regset.size(); ++i)
                builder_.add(dest_regset[i], lhs_regset[i], rhs_regset[i]);
        } else {
            if (op_type.width() == 1) {
                fill_byte_with_bit(builder_, lhs_regset);
                fill_byte_with_bit(builder_, rhs_regset);
                op_type = ir::value_type(op_type.type_class(), 32, 1);
            }

            // Scalar addition (including > 64-bits)
            auto shift_op = extend_register(builder_, lhs_regset[0], op_type);
            builder_.adds(dest_regset[0], lhs_regset[0], rhs_regset[0], shift_op);

            // Addition for > 64-bits
            for (std::size_t i = 1; i < dest_regset.size(); ++i)
                builder_.adcs(dest_regset[i], lhs_regset[i], rhs_regset[i]);
        }
        break;
	case binary_arith_op::sub:
        // Vector subtraction
        if (is_vector_op) {
            sets_flags = false;
            for (std::size_t i = 0; i < dest_regset.size(); ++i)
                builder_.sub(dest_regset[i], lhs_regset[i], rhs_regset[i]);
            break;
        } else {
            // Flag
            if (op_type.width() == 1) {
                fill_byte_with_bit(builder_, lhs_regset);
                fill_byte_with_bit(builder_, rhs_regset);
                op_type = ir::value_type(op_type.type_class(), 32, 1);
            }

            // Scalar subtraction (including > 64-bits)
            auto shift_op = extend_register(builder_, lhs_regset[0], op_type);
            if (dest_regset[0].type().is_floating_point()) {
                dest_regset[0].cast(ir::value_type::f128());
                lhs_regset[0].cast(ir::value_type::f128());
                rhs_regset[0].cast(ir::value_type::f128());
            }
            builder_.subs(dest_regset[0], lhs_regset[0], rhs_regset[0], shift_op);

            // Subtraction for > 64-bits
            for (std::size_t i = 1; i < dest_regset.size(); ++i)
                builder_.sbcs(dest_regset[i], lhs_regset[i], rhs_regset[i]);

            // This is only available with the +flagm architecture option
            // TODO: make it enabled in those cases
            // builder_.cfinv("invert carry flag (to match x86 semantics)");
        }
        inverse_carry_flag_operation = true;
        break;
	case binary_arith_op::mul:
        mul_impl(dest_regset, lhs_regset, rhs_regset);
        break;
	case binary_arith_op::div:
        div_impl(dest_regset, lhs_regset, rhs_regset);
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
            auto temp1_regset = vreg_alloc_.allocate(op_type);
            auto temp2_regset = vreg_alloc_.allocate(op_type);
            div_impl(temp1_regset, lhs_regset, rhs_regset);
            mul_impl(temp2_regset, rhs_regset, temp1_regset);

            // NOTE: no register extensions needed; since mod operates on >= 64-bit virtual registers only
            builder_.subs(dest_regset[0], lhs_regset[0], temp2_regset[0]);
            for (std::size_t i = 1; i < dest_regset.size(); ++i) {
                builder_.sbcs(dest_regset[i], lhs_regset[i], temp2_regset[i]);
            }

            sets_flags = false;
        }
		break;
	case binary_arith_op::bor:
        if (is_vector_op || n.val().type().element_width() > 64) {
            for (std::size_t i = 0; i < dest_regset.size(); ++i) {
                builder_.orr_(dest_regset[i], lhs_regset[i], rhs_regset[i]);
            }
            sets_flags = false;
            break;
        }

        switch (op_type.element_width()) {
        case 1:
            fill_byte_with_bit(builder_, lhs_regset);
            fill_byte_with_bit(builder_, rhs_regset);
        case 8:
        case 16:
            extend_register(builder_, lhs_regset, op_type);
            extend_register(builder_, rhs_regset, op_type);
        case 32:
            builder_.orr_(dest_regset, lhs_regset, rhs_regset);
            builder_.ands(register_operand(register_operand::wzr_sp), dest_regset, dest_regset);
            break;
        case 64:
            builder_.orr_(dest_regset, lhs_regset, rhs_regset);
            builder_.ands(register_operand(register_operand::xzr_sp), dest_regset, dest_regset);
            break;
        default:
            throw backend_exception("Unsupported ORR operation between {} x {}",
                                    n.lhs().type(), n.rhs().type());
        }
        builder_.setz(flag_map[reg_offsets::ZF]).add_comment("compute flag: ZF");
        builder_.cset(flag_map[reg_offsets::SF], cond_operand::mi()).add_comment("compute flag: SF");
        sets_flags = false;
        if (lhs_regset[0].type().element_width() < 64) {
            unsigned long long mask = ~(~0llu << lhs_regset[0].type().element_width());
            builder_.and_(dest_regset, dest_regset, builder_.move_to_register(mask, dest_regset[0].type()));
        }
		break;
	case binary_arith_op::band:
        if (is_vector_op || n.val().type().element_width() > 64) {
            for (std::size_t i = 0; i < dest_regset.size(); ++i) {
                builder_.ands(dest_regset[i], lhs_regset[i], rhs_regset[i]);
            }
            sets_flags = false;
            break;
        }

        switch (op_type.element_width()) {
        case 1:
            fill_byte_with_bit(builder_, lhs_regset);
            fill_byte_with_bit(builder_, rhs_regset);
        case 8:
        case 16:
            extend_register(builder_, lhs_regset, op_type);
            extend_register(builder_, rhs_regset, op_type);
        case 32:
        case 64:
            builder_.ands(dest_regset, lhs_regset, rhs_regset);
            break;
        default:
            throw backend_exception("Unsupported AND operation between {} x {}",
                                    n.lhs().type(), n.rhs().type());
        }
        builder_.setz(flag_map[reg_offsets::ZF]).add_comment("compute flag: ZF");
        builder_.cset(flag_map[reg_offsets::SF], cond_operand::mi()).add_comment("compute flag: SF");
        sets_flags = false;
        if (lhs_regset[0].type().element_width() < 64) {
            unsigned long long mask = ~(~0llu << lhs_regset[0].type().element_width());
            builder_.and_(dest_regset, dest_regset, builder_.move_to_register(mask, dest_regset[0].type()));
        }
		break;
	case binary_arith_op::bxor:
        {
            if (n.val().type().is_floating_point()) {
                lhs_regset[0].cast(value_type::vector(value_type::f64(), 2));
                rhs_regset[0].cast(value_type::vector(value_type::f64(), 2));
                auto type = dest_regset[0].type();
                dest_regset[0].cast(value_type::vector(value_type::f64(), 2));
                builder_.eor_(dest_regset[0], lhs_regset[0], rhs_regset[0]);
                dest_regset[0].cast(type);
                std::size_t i = 1;
                for (; i < dest_regset.size(); ++i) {
                    lhs_regset[i].cast(value_type::vector(value_type::f64(), 2));
                    rhs_regset[i].cast(value_type::vector(value_type::f64(), 2));
                    dest_regset[i].cast(value_type::vector(value_type::f64(), 2));
                    builder_.eor_(dest_regset[i], lhs_regset[i], rhs_regset[i]);
                    dest_regset[i].cast(type);
                }
                sets_flags = false;
                break;
            }

            builder_.eor_(dest_regset[0], lhs_regset[0], rhs_regset[0]);
            std::size_t i = 1;
            for (; i < dest_regset.size(); ++i) {
                builder_.eor_(dest_regset[i], lhs_regset[i], rhs_regset[i]);
            }

            // EOR does not set flags
            // TODO
            if (is_vector_op) sets_flags = false;
            else {
                if (dest_regset[i-1].type().element_width() > 32)
                    builder_.ands(register_operand(register_operand::xzr_sp), dest_regset[i-1], dest_regset[i-1]);
                else
                    builder_.ands(register_operand(register_operand::wzr_sp), dest_regset[i-1], dest_regset[i-1]);
            }
        }
        builder_.setz(flag_map[reg_offsets::ZF]).add_comment("compute flag: ZF");
        builder_.cset(flag_map[reg_offsets::SF], cond_operand::mi()).add_comment("compute flag: SF");
        sets_flags = false;
        if (lhs_regset[0].type().element_width() < 64) {
            unsigned long long mask = ~(~0llu << lhs_regset[0].type().element_width());
            builder_.and_(dest_regset, dest_regset, builder_.move_to_register(mask, dest_regset[0].type()));
        }
        break;
	case binary_arith_op::cmpeq:
	case binary_arith_op::cmpne:
	case binary_arith_op::cmpgt:
        // TODO: this looks wrong
        if (is_vector_op || dest_regset.size() > 1)
            throw backend_exception("Unsupported comparison between {} x {}",
                                    n.lhs().type(), n.rhs().type());

        rhs_regset[0] = cast(rhs_regset[0], lhs_regset[0].type());
        builder_.insert_comment("Compare LHS and RHS to generate condition for conditional set");
        builder_.compare(variable(lhs_regset), variable(rhs_regset));
        builder_.cset(dest_regset[0], get_cset_type(n.op()))
                .add_comment("set to 1 if condition is true (based flags from the previous compare)");
        inverse_carry_flag_operation = true;
		break;
	case binary_arith_op::cmpoeq:
	case binary_arith_op::cmpolt:
	case binary_arith_op::cmpole:
	case binary_arith_op::cmpo:
	case binary_arith_op::cmpu:
        builder_.fcmp(lhs_regset, rhs_regset);
        dest_regset[0].cast(value_type::u64());
        builder_.cset(dest_regset[0], get_cset_type(n.op()))
                .add_comment("set to 1 if condition is true (based flags from the previous compare)");
        break;
	case binary_arith_op::cmpueq:
	case binary_arith_op::cmpune:
	case binary_arith_op::cmpult:
	case binary_arith_op::cmpunlt:
	case binary_arith_op::cmpunle:
        {
            builder_.fcmp(lhs_regset, rhs_regset);
            const auto& unordered = vreg_alloc_.allocate(value_type::u64());
            dest_regset[0].cast(value_type::u64());
            builder_.cset(dest_regset[0], get_cset_type(n.op()))
                    .add_comment("set to 1 if condition is true (based flags from the previous compare)");
            builder_.cset(unordered, cond_operand::vs())
                    .add_comment("set to 1 if condition is true (based flags from the previous compare)");
            builder_.orr_(dest_regset[0], dest_regset[0], unordered);
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
        builder_.setz(flag_map[reg_offsets::ZF]).add_comment("compute flag: ZF");
        builder_.sets(flag_map[reg_offsets::SF]).add_comment("compute flag: SF");
        builder_.seto(flag_map[reg_offsets::OF]).add_comment("compute flag: OF");

        // ARM computes flags in the same way as x86 for subtraction
        // EXCEPT for the CF; which is cleared when there is underflow and set otherwise (the
        // opposite behaviour to x86)
        if (inverse_carry_flag_operation)
            builder_.setcc(flag_map[reg_offsets::CF]).add_comment("compute flag: CF");
        else
            builder_.setc(flag_map[reg_offsets::CF]).add_comment("compute flag: CF");
    }
}

void arm64_translation_context::materialise_ternary_arith(const ternary_arith_node &n) {
    allocate_flags(vreg_alloc_, flag_map, n);

	const auto &dest_regs = vreg_alloc_.allocate(n.val());
    const auto &lhs_regs = materialise_port(n.lhs());
    const auto &rhs_regs = materialise_port(n.rhs());
    auto &top_regs = materialise_port(n.top());

    // Sanity check
    // Binary operations are defined in the IR with same size inputs and output
    [[unlikely]]
    if (n.lhs().type() != n.rhs().type() || n.lhs().type() != n.val().type()
                                         || n.lhs().type() != n.top().type())
    {
        throw backend_exception("Binary operations not supported between types {} = {} op {}",
                                n.val().type(), n.lhs().type(), n.rhs().type());
    }

    [[unlikely]]
    if (lhs_regs.size() != rhs_regs.size() || lhs_regs.size() != dest_regs.size()
                                           || lhs_regs.size() != top_regs.size())
    {
        throw backend_exception("Binary operations not supported between types {} = {} op {}",
                                n.val().type(), n.lhs().type(), n.rhs().type());
    }

    bool inverse_carry_flag_operation = false;
    const register_operand& pstate = vreg_alloc_.allocate(register_operand(register_operand::nzcv).type());
    for (std::size_t i = 0; i < dest_regs.size(); ++i) {
        // Set carry flag
        // builder_.mrs(pstate, register_operand(register_operand::nzcv));
        // builder_.lsl(top_regs[i], top_regs[i], 0x3);
        //
        // top_regs[i] = cast(top_regs[i], pstate.type());
        // builder_.orr_(pstate, pstate, top_regs[i]);
        // builder_.msr(register_operand(register_operand::nzcv), pstate);

        builder_.compare(variable(top_regs[i]), 0);
        builder_.sbcs(register_operand(register_operand::wzr_sp),
                      register_operand(register_operand::wzr_sp),
                      register_operand(register_operand::wzr_sp));
        switch (n.op()) {
        case ternary_arith_op::adc:
            builder_.adcs(dest_regs[i], lhs_regs[i], rhs_regs[i]);
            break;
        case ternary_arith_op::sbb:
            builder_.sbcs(dest_regs[i], lhs_regs[i], rhs_regs[i]);
            inverse_carry_flag_operation = true;
            break;
        default:
            throw backend_exception("Unsupported ternary arithmetic operation {}", util::to_underlying(n.op()));
        }
    }

	builder_.setz(flag_map[reg_offsets::ZF]).add_comment("compute flag: ZF");
	builder_.sets(flag_map[reg_offsets::SF]).add_comment("compute flag: SF");
	builder_.seto(flag_map[reg_offsets::OF]).add_comment("compute flag: OF");
    if (inverse_carry_flag_operation)
        builder_.setcc(flag_map[reg_offsets::CF]).add_comment("compute flag: CF");
    else
        builder_.setc(flag_map[reg_offsets::CF]).add_comment("compute flag: CF");
}

void arm64_translation_context::materialise_binary_atomic(const binary_atomic_node &n) {
	const auto &dest_vregs = vreg_alloc_.allocate(n.val());
    const auto &src_vregs = materialise_port(n.rhs());
    const auto &addr_regs = materialise_port(n.address());

    [[unlikely]]
    if (addr_regs.size() != 1)
        throw backend_exception("Binary atomic operation address type not supported {}", n.address().type());

    [[unlikely]]
    if (dest_vregs.size() != 1 || src_vregs.size() != 1)
        throw backend_exception("Binary atomic operations not supported for vector types (src: {} or dest: {})",
                                n.rhs().type(), n.val().type());

    const auto &src_vreg = src_vregs[0];
    const auto &dest_vreg = dest_vregs[0];

    // No need to handle flags: they are not visible to other PEs
    allocate_flags(vreg_alloc_, flag_map, n);

    memory_operand mem_addr(addr_regs[0]);

    bool sets_flags = true;
    bool inverse_carry_flag_operation = false;

    // FIXME: correct memory ordering?
    // NOTE: not sure if the proper alternative was used (should a/al/l or
    // nothing be used?)

    atomic_block atomic(builder_, instr_cnt_, mem_addr);
    const register_operand& status = vreg_alloc_.allocate(value_type::u32());
    if constexpr (!supports_lse) {
        atomic.start_atomic_block(dest_vreg);
    }

	switch (n.op()) {
	case binary_atomic_op::add:
        builder_.adds(dest_vreg, dest_vreg, src_vreg);
        break;
	case binary_atomic_op::sub:
        builder_.subs(dest_vreg, dest_vreg, src_vreg);
        inverse_carry_flag_operation = true;
        break;
    case binary_atomic_op::xadd:
        {
            // TODO: must find a way to make this unique
            const register_operand &old_dest = vreg_alloc_.allocate(n.rhs().type());
            builder_.adds(dest_vreg, dest_vreg, src_vreg);
            builder_.mov(src_vreg, old_dest);
        }
        break;
	case binary_atomic_op::bor:
        builder_.orr_(dest_vreg, dest_vreg, src_vreg);
        builder_.compare(variable(dest_vreg), 0);
        inverse_carry_flag_operation = true;
		break;
	case binary_atomic_op::band:
        // TODO: Not sure if this is correct
        builder_.ands(dest_vreg, dest_vreg, src_vreg);
		break;
	case binary_atomic_op::bxor:
        builder_.eor_(dest_vreg, dest_vreg, src_vreg);
        builder_.compare(variable(dest_vreg), 0);
        inverse_carry_flag_operation = true;
		break;
    case binary_atomic_op::btc:
        builder_.mov(dest_vreg, 0);
        sets_flags = false;
		break;
    case binary_atomic_op::bts:
        builder_.mov(dest_vreg, 0);
        sets_flags = false;
		break;
    case binary_atomic_op::xchg:
        // TODO: check if this works
        builder_.mov(dest_vreg, src_vreg);
        sets_flags = false;
        break;
	default:
		throw backend_exception("unsupported binary atomic operation {}", util::to_underlying(n.op()));
	}

    if constexpr (!supports_lse) {
        atomic.end_atomic_block(status, dest_vreg);
    }

    if (sets_flags) {
        builder_.setz(flag_map[reg_offsets::ZF]).add_comment("write flag: ZF");
        builder_.sets(flag_map[reg_offsets::SF]).add_comment("write flag: SF");
        builder_.seto(flag_map[reg_offsets::OF]).add_comment("write flag: OF");

        if (inverse_carry_flag_operation)
            builder_.setcc(flag_map[reg_offsets::CF]).add_comment("compute flag: CF");
        else
            builder_.setc(flag_map[reg_offsets::CF]).add_comment("compute flag: CF");
    }
}

void arm64_translation_context::materialise_ternary_atomic(const ternary_atomic_node &n) {
    // Destination register only used for storing return code of STXR (a 32-bit value)
    // Since STXR expects a 32-bit register; we directly allocate a 32-bit one
    // NOTE: we're only going to use it afterwards for a comparison and an increment
    register_operand status_reg = vreg_alloc_.allocate(value_type::u32());
    const register_operand &current_data_reg = vreg_alloc_.allocate(n.val(), n.rhs().type());

    const register_operand &acc_reg = materialise_port(n.rhs());
    const register_operand &src_reg = materialise_port(n.top());
    const register_operand &addr_reg = materialise_port(n.address());

    allocate_flags(vreg_alloc_, flag_map, n);

    // CMPXCHG:
    // dest_reg = mem[addr];
    // if (dest_reg != acc_reg) acc_reg = dest_reg;
    // else try mem[addr] = src_reg;
    //      if (failed) goto beginning
    // end
    auto mem_addr = memory_operand(addr_reg);
    switch (n.op()) {
    case ternary_atomic_op::cmpxchg:
        if constexpr (supports_lse) {
            builder_.insert_comment("Atomic CMPXCHG using CAS (enabled on systems with LSE support");
            builder_.cas(acc_reg, src_reg, memory_operand(mem_addr))
                    .add_comment("write source (2nd reg) to memory if source == accumulator (1st reg), accumulator = source");
            builder_.compare(variable(acc_reg), 0);
        } else {
            builder_.insert_comment("Atomic CMPXCHG without CAS");
            atomic_block atomic{builder_, instr_cnt_, mem_addr};
            atomic.start_atomic_block(current_data_reg);
            builder_.insert_comment("Compare with accumulator");
            builder_.compare(variable(current_data_reg), variable(acc_reg));
            builder_.csel(acc_reg, current_data_reg, acc_reg, cond_operand::ne())
                     .add_comment("conditionally move current memory value into accumulator");
            atomic.end_atomic_block(status_reg, src_reg);
        }
        break;
    default:
		throw backend_exception("unsupported binary atomic operation {}", util::to_underlying(n.op()));
    }

    builder_.setz(flag_map[reg_offsets::ZF]).add_comment("compute flag: ZF");
    builder_.sets(flag_map[reg_offsets::SF]).add_comment("compute flag: SF");
    builder_.seto(flag_map[reg_offsets::OF]).add_comment("compute flag: OF");
    builder_.setc(flag_map[reg_offsets::CF]).add_comment("compute flag: CF");
}

void arm64_translation_context::materialise_unary_arith(const unary_arith_node &n) {
    const auto &dest_vregs = vreg_alloc_.allocate(n.val());
    const auto &lhs_vregs = materialise_port(n.lhs());

    switch (n.op()) {
    case unary_arith_op::bnot:
        // TODO: replace this with just inverse when we start directly allocating variables
        if (is_flag_port(n.val()))
            builder_.append(arm64_assembler::eor(dest_vregs[0], lhs_vregs[0], 1));
        else
            builder_.inverse(variable(dest_vregs), variable(lhs_vregs));
        break;
    case unary_arith_op::neg:
        // neg: ~reg + 1 for complement-of-2
        builder_.negate(variable(dest_vregs), variable(lhs_vregs));
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
        builder_.sign_extend(variable(dest_vregs), variable(src_vregs));
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
        builder_.zero_extend(variable(dest_vregs), variable(src_vregs));
        return;
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

    builder_.insert_comment("compare condition for conditional select");
    builder_.compare(variable(condition), 0);
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
    amount = cast(amount, n.val().type());

    const auto& dest_vreg = vreg_alloc_.allocate(n.val());
    logger.debug("Handling bit-shift from {} by {} to {}\n", n.input().type(), n.amount().type(), n.val().type());

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
        auto out = cast(insertion_bits, dest[0].type());
        builder_.bfi(dest, out, insert_idx, insert_len);
        return;
    }

    std::size_t bits_idx = 0;
    std::size_t bits_total_width = total_width(insertion_bits);

    std::size_t inserted = 0;
    std::size_t insert_start = n.to() / dest[0].type().element_width();
    for (std::size_t i = insert_start; inserted < n.length(); ++i) {
        auto out = cast(insertion_bits[bits_idx], dest[i].type());

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
        const auto &value_vreg = cast(value_vregs[i], dest_vregs[index + i].type());
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
        const auto &src_vreg = cast(src_vregs[index+i], dest_vregs[i].type());
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

