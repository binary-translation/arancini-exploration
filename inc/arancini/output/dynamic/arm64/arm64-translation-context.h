#pragma once

#include "arancini/ir/value-type.h"
#include "arancini/output/dynamic/arm64/arm64-instruction.h"
#include "arm64-instruction-builder.h"
#include <arancini/output/dynamic/arm64/arm64-common.h>

#include <arancini/ir/node.h>
#include <arancini/ir/port.h>
#include <arancini/output/dynamic/translation-context.h>

#include <unordered_map>
#include <unordered_set>

namespace arancini::output::dynamic::arm64 {

class register_sequence {
public:
    // TODO: do we need this?
    register_sequence() = default;

    register_sequence(const register_operand& reg):
        regs_{reg}
    { }

    register_sequence(std::initializer_list<register_operand> regs):
        regs_(regs)
    { }

    template <typename It>
    register_sequence(It begin, It end):
        regs_(begin, end)
    { }

    operator register_operand() const {
        [[unlikely]]
        if (regs_.size() > 1)
            throw backend_exception("Accessing register set of {} registers as single register",
                                    regs_.size());
        return regs_[0];
    }

    operator std::vector<register_operand>() const {
        return regs_;
    }

    [[nodiscard]]
    register_operand& operator[](std::size_t i) { return regs_[i]; }

    [[nodiscard]]
    const register_operand& operator[](std::size_t i) const { return regs_[i]; }

    [[nodiscard]]
    register_operand& front() { return regs_[0]; }

    [[nodiscard]]
    const register_operand& front() const { return regs_[0]; }

    [[nodiscard]]
    register_operand& back() { return regs_[regs_.size()]; }

    [[nodiscard]]
    const register_operand& back() const { return regs_[regs_.size()]; }

    [[nodiscard]]
    std::size_t size() const { return regs_.size(); }
private:
    std::vector<register_operand> regs_;
};

class virtual_register_allocator {
public:
    [[nodiscard]]
	register_sequence allocate(ir::value_type type) {
        return register_sequence{register_operand(next_vreg_++, type)};
    }

    register_sequence& allocate(const ir::port& p) {
        if (base_representable(p.type()))
            return allocate(p, p.type());
        return allocate_sequence(p);
    }

	register_sequence &allocate(const ir::port &p, ir::value_type type) {
		auto v = allocate(type);
        port_to_vreg_[&p] = v;
		return port_to_vreg_[&p];
	}

    [[nodiscard]]
    register_sequence& get(const ir::port& p) { return port_to_vreg_[&p]; }

    void reset() { next_vreg_ = 33; port_to_vreg_.clear(); }
private:
    std::size_t next_vreg_ = 33; // TODO: formalize this
	std::unordered_map<const ir::port *, register_sequence> port_to_vreg_;

    register_sequence& allocate_sequence(const ir::port& p);

    bool base_representable(const ir::value_type& type) {
        return type.width() <= 64; // TODO: formalize this
    }
};

class arm64_translation_context : public translation_context {
public:
	arm64_translation_context(machine_code_writer &writer)
		: translation_context(writer)
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

    virtual_register_allocator vreg_alloc_;

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

    memory_operand guestreg_memory_operand(int regoff,
                                           memory_operand::address_mode mode = memory_operand::address_mode::direct);

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
    register_operand mov_immediate(T imm, ir::value_type type);

    register_operand cast(const register_operand &op, ir::value_type type);
};

} // namespace arancini::output::dynamic::arm64

