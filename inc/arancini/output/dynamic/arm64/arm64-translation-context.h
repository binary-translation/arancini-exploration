#pragma once

#include <arancini/ir/value-type.h>
#include <arancini/input/registers.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>
#include <arancini/output/dynamic/arm64/arm64-instruction-builder.h>

#include <arancini/ir/node.h>
#include <arancini/ir/port.h>
#include <arancini/output/dynamic/translation-context.h>

#include <type_traits>
#include <unordered_map>

namespace arancini::output::dynamic::arm64 {

static constexpr ir::value_type base_type{ir::value_type_class::unsigned_integer, 64};
static constexpr ir::value_type host_address_type = base_type;

static constexpr register_operand memory_base_reg{register_operand::x28};
static constexpr register_operand context_block_reg{register_operand::x29};

class virtual_register_allocator final {
public:
    using size_type = std::size_t;
    using register_sequence = std::vector<register_operand>;

    // Allocates a new virtual register of specified type
    [[nodiscard]]
    register_operand allocate(ir::value_type type) {
        return register_operand{next_vreg_++, type};
    }

    // Allocates a set of new virtual register for particular port
    [[nodiscard]]
    register_sequence allocate(const ir::port &p) {
        auto reg_count = p.type().nr_elements();
        auto element_width = p.type().width();
        if (element_width > base_type.element_width()) {
            element_width = base_type.element_width();

            // TODO: revise this; division slow
            reg_count = p.type().width() / element_width;
        }

        auto type = ir::value_type(p.type().type_class(), element_width, 1);
        return allocate_all_to_port(p, reg_count, type);
    }

    // Allocates a new virtual register and associates with given port
    register_operand allocate_to_port(const ir::port &p) {
        return allocate_to_port(p, p.type());
    }

    // Allocates a new virtual register of particular type and associates with given port
    register_operand allocate_to_port(const ir::port &p, ir::value_type type) {
        return match_to_port(p, allocate(type));
    }

    // Associates existing virtual register to port
    register_operand match_to_port(const ir::port &p, register_operand reg_op) {
        if (!reg_op.is_virtual())
            throw arm64_exception("Cannot match physical register {} to port through virtual register allocator",
                                  reg_op);
        port_to_vreg_[&p].emplace_back(reg_op);
		return reg_op;
    }

    // Retrieves allocations for port (if the port has allocations)
    [[nodiscard]]
    const register_sequence &at(const ir::port &p) const {
        return port_to_vreg_.at(&p);
    }

    // Retrieves current virtual registers allocated
    [[nodiscard]]
    size_type current_allocations() const { return next_vreg_; }

    // Eliminates all allocations
    void clear() { port_to_vreg_.clear(); }
private:
    size_type next_vreg_ = 0;
    std::unordered_map<const ir::port *, register_sequence> port_to_vreg_;

    const register_sequence &allocate_all_to_port(const ir::port &p, size_type count, ir::value_type type) {
        port_to_vreg_[&p].reserve(count);
        for (size_type i = 0; i < count; ++i)
            port_to_vreg_[&p].push_back(allocate(type));
        return port_to_vreg_[&p];
    }
};

class arm64_translation_context final : public translation_context {
public:
	arm64_translation_context(machine_code_writer &writer);

	virtual void begin_block() override;
	virtual void begin_instruction(off_t address, const std::string &disasm) override;
	virtual void end_instruction() override;
	virtual void end_block() override;
	virtual void lower(const std::shared_ptr<ir::action_node> &n) override;

    virtual ~arm64_translation_context() { }
private:
	instruction_builder builder_;
    virtual_register_allocator vreg_allocator_;

    std::vector<ir::node *> nodes_;
	std::set<const ir::node *> materialised_nodes_;
	std::unordered_map<unsigned long, off_t> instruction_index_to_guest_;
    std::unordered_map<const ir::local_var *, std::vector<register_operand>> locals_;
    int ret_;
	off_t this_pc_;
    size_t instr_cnt_ = 0;

    std::vector<register_operand> &materialise_port(ir::port &p);

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>>>
    [[nodiscard]]
    memory_operand guest_register_accessor(T guest_register_byte_offset,
                                           memory_operand::addressing_modes mode = memory_operand::addressing_modes::indirect)
    {
        // If immediate offset fits in 12-bits; access using
        [[likely]]
        if (immediate_operand::fits(guest_register_byte_offset, u12())) {
            return {context_block_reg, immediate_operand(guest_register_byte_offset, u12()), mode};
        }

        auto base_vreg = vreg_allocator_.allocate(host_address_type);
        builder_.mov(base_vreg, immediate_operand(guest_register_byte_offset, u12()));
        builder_.add(base_vreg, context_block_reg, base_vreg);
        return {base_vreg, immediate<0, 12>(), mode};
    }

    void materialise(const ir::node *n);
    void materialise_read_reg(const ir::read_reg_node &n);
    void materialise_write_reg(const ir::write_reg_node &n);
    void materialise_read_mem(const ir::read_mem_node &n);
    void materialise_write_mem(const ir::write_mem_node &n);
    void materialise_read_pc(const ir::read_pc_node &n);
    void materialise_write_pc(const ir::write_pc_node &n);
    void materialise_label(const ir::label_node &n);
    void materialise_br(const ir::br_node &n);
    void materialise_cond_br(const ir::cond_br_node &n);
    void materialise_cast(const ir::cast_node &n);
    void materialise_constant(const ir::constant_node &n);
    void materialise_csel(const ir::csel_node &n);
    void materialise_bit_shift(const ir::bit_shift_node &n);
    void materialise_bit_extract(const ir::bit_extract_node &n);
    void materialise_bit_insert(const ir::bit_insert_node &n);
    void materialise_vector_insert(const ir::vector_insert_node &n);
    void materialise_vector_extract(const ir::vector_extract_node &n);
    void materialise_unary_arith(const ir::unary_arith_node &n);
    void materialise_binary_arith(const ir::binary_arith_node &n);
    void materialise_ternary_arith(const ir::ternary_arith_node &n);
    void materialise_binary_atomic(const ir::binary_atomic_node &n);
    void materialise_ternary_atomic(const ir::ternary_atomic_node &n);
    void materialise_internal_call(const ir::internal_call_node &n);
    void materialise_read_local(const ir::read_local_node &n);
    void materialise_write_local(const ir::write_local_node &n);

    register_operand add_membase(const register_operand &addr, const ir::value_type &t = ir::value_type::u64());

    template <typename T, std::enable_if_t<std::is_arithmetic<T>::value, int> = 0>
    std::variant<register_operand, immediate_operand> move_immediate(T imm, ir::value_type type);

    template <typename T, std::enable_if_t<std::is_arithmetic<T>::value, int> = 0>
    register_operand move_as_register(T imm, ir::value_type type);

    register_operand cast(const register_operand &op, ir::value_type type);

    util::static_map<input::x86::reg_idx, register_operand, 4> flag_map;
};

} // namespace arancini::output::dynamic::arm64

