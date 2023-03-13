#pragma once

#include "arm64-instruction-builder.h"

#include <arancini/ir/node.h>
#include <arancini/ir/port.h>
#include <arancini/output/dynamic/translation-context.h>

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
	arm64_instruction_builder builder_;
    std::vector<std::shared_ptr<ir::action_node>> nodes_;
	std::set<const ir::node *> materialised_nodes_;
	std::map<const ir::port *, int> port_to_vreg_;
	std::map<unsigned long, off_t> instruction_index_to_guest_;
    int ret_;
	int next_vreg_;
	off_t this_pc_;
    size_t instr_cnt_ = 0;

	void do_register_allocation();

	int alloc_vreg() { return next_vreg_++; }

	int alloc_vreg_for_port(const ir::port &p)
	{
		int v = alloc_vreg();
		port_to_vreg_[&p] = v;
		return v;
	}

	arm64_operand vreg_operand_for_port(ir::port &p, bool constant_fold = false);
	int vreg_for_port(ir::port &p) const { return port_to_vreg_.at(&p); }

    arm64_operand guestreg_memory_operand(int width, int regoff,
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
    void materialise_internal_call(const ir::internal_call_node &n);

    arm64_operand mov_immediate(uint64_t imm, uint8_t size);
};
} // namespace arancini::output::dynamic::arm64

