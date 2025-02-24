#pragma once

#include <arancini/ir/value-type.h>
#include <arancini/input/registers.h>
#include <arancini/output/dynamic/arm64/arm64-common.h>
#include <arancini/output/dynamic/arm64/arm64-instruction-builder.h>

#include <arancini/ir/node.h>
#include <arancini/ir/port.h>
#include <arancini/output/dynamic/translation-context.h>

#include <unordered_map>
#include <unordered_set>

namespace arancini::output::dynamic::arm64 {

class port_register_allocator final {
public:
    port_register_allocator(virtual_register_allocator* regalloc):
        regalloc_(regalloc)
    { }

    [[nodiscard]]
    register_sequence allocate(ir::value_type type) {
        return regalloc_->allocate(type);
    }

    register_sequence& allocate(const ir::port& p) {
        port_to_vreg_[&p] = regalloc_->allocate(p.type());
        return port_to_vreg_[&p];
    }

	register_sequence &allocate(const ir::port &p, ir::value_type type) {
        port_to_vreg_[&p] = regalloc_->allocate(type);
		return port_to_vreg_[&p];
	}

    [[nodiscard]]
    register_sequence& get(const ir::port& p) { return port_to_vreg_[&p]; }

    void reset() { regalloc_->reset(); port_to_vreg_.clear(); }
private:
    virtual_register_allocator* regalloc_;
	std::unordered_map<const ir::port *, register_sequence> port_to_vreg_;
};

class arm64_translation_context final : public translation_context {
public:
	arm64_translation_context(machine_code_writer &writer):
        translation_context(writer)
	{ }

	virtual void begin_block() override;
	virtual void begin_instruction(off_t address, const std::string &disasm) override;
	virtual void end_instruction() override;
	virtual void end_block() override;
	virtual void lower(const std::shared_ptr<ir::action_node> &n) override;

    void reset_context();

    virtual ~arm64_translation_context() { }
private:
	instruction_builder builder_;
    std::vector<ir::node *> nodes_;
	std::unordered_set<const ir::node *> materialised_nodes_;
	std::unordered_map<unsigned long, off_t> instruction_index_to_guest_;
    std::unordered_map<const ir::local_var *, std::vector<register_operand>> locals_;

    port_register_allocator vreg_alloc_{&builder_.register_allocator()};

    int ret_;
	off_t this_pc_;
    std::size_t instr_cnt_ = 0;

    // TODO: this should be included only when debugging is enabled
    std::string current_instruction_disasm_;

    [[nodiscard]]
    register_sequence& materialise_port(const ir::port &p) {
        materialise(p.owner());
        return vreg_alloc_.get(p);
    }

    using address_mode = memory_operand::address_mode;

    [[nodiscard]]
    memory_operand guest_memory(int regoff, address_mode mode = address_mode::direct);

    [[nodiscard]]
    memory_operand guest_memory(arancini::input::x86::reg_offsets regoff, address_mode mode = address_mode::direct) {
        return guest_memory(static_cast<int>(regoff), mode);
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
};

} // namespace arancini::output::dynamic::arm64

