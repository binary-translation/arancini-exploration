#pragma once

#include "arancini/output/dynamic/arm64/arm64-instruction.h"
#include "arm64-instruction-builder.h"

#include <arancini/ir/node.h>
#include <arancini/ir/port.h>
#include <arancini/output/dynamic/translation-context.h>

#include <unordered_map>

namespace arancini::output::dynamic::arm64 {
class arm64_translation_context : public translation_context {
public:
	arm64_translation_context(machine_code_writer &writer)
		: translation_context(writer)
	{
	}

	virtual void begin_block() override;
	virtual void begin_instruction(off_t address, const std::string &disasm) override;
	virtual void end_instruction() override;
	virtual void end_block() override;
	virtual void lower(const std::shared_ptr<ir::action_node> &n) override;

    virtual ~arm64_translation_context() { }
private:
	instruction_builder builder_;
    std::vector<ir::node *> nodes_;
	std::set<const ir::node *> materialised_nodes_;
	std::unordered_map<const ir::port *, std::vector<vreg_operand>> port_to_vreg_;
	std::unordered_map<unsigned long, off_t> instruction_index_to_guest_;
    std::unordered_map<const ir::local_var *, std::vector<vreg_operand>> locals_;
    int ret_;
	int next_vreg_;
	off_t this_pc_;
    size_t instr_cnt_ = 0;

	vreg_operand alloc_vreg(ir::value_type type) {
        return vreg_operand(next_vreg_++, type);
    }

	vreg_operand alloc_vreg(const ir::port &p, ir::value_type type) {
		auto v = alloc_vreg(type);
        if (port_to_vreg_.find(&p) == port_to_vreg_.end())
            port_to_vreg_[&p] = {v};
        else
            port_to_vreg_[&p].push_back(v);
		return v;
	}

    vreg_operand alloc_vreg(const ir::port &p) {
        return alloc_vreg(p, p.type());
    }

	vreg_operand vreg_for_port(const ir::port &p, size_t index = 0) const {
        return port_to_vreg_.at(&p)[index];
    }

    std::vector<vreg_operand> vregs_for_port(const ir::port &p) const {
        return port_to_vreg_.at(&p);
    }

    std::vector<vreg_operand> materialise_port(ir::port &p, bool constant_fold = false);


    memory_operand guestreg_memory_operand(int regoff,
                                           bool pre = false,
                                           bool post = false);

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
    void materialise_unary_arith(const ir::unary_arith_node &n);
    void materialise_binary_arith(const ir::binary_arith_node &n);
    void materialise_binary_atomic(const ir::binary_atomic_node &n);
    void materialise_ternary_atomic(const ir::ternary_atomic_node &n);
    void materialise_internal_call(const ir::internal_call_node &n);
    void materialise_read_local(const ir::read_local_node &n);
    void materialise_write_local(const ir::write_local_node &n);

    vreg_operand add_membase(const vreg_operand &addr);
    vreg_operand mov_immediate(uint64_t imm, arancini::ir::value_type type);
};
} // namespace arancini::output::dynamic::arm64

