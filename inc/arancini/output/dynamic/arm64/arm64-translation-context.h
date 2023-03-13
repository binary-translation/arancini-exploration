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
	virtual void lower(ir::node *n) override;

    virtual ~arm64_translation_context() { }
private:
	arm64_instruction_builder builder_;
	std::set<ir::node *> materialised_nodes_;
	std::map<const ir::port *, int> port_to_vreg_;
	std::map<unsigned long, off_t> instruction_index_to_guest_;
	int next_vreg_;
	off_t this_pc_;

	void do_register_allocation();

	int alloc_vreg() { return next_vreg_++; }

	int alloc_vreg_for_port(const ir::port &p)
	{
		int v = alloc_vreg();
		port_to_vreg_[&p] = v;
		return v;
	}

	// operand operand_for_port(ir::port &p);
	// operand vreg_operand_for_port(ir::port &p);
	arm64_operand vreg_operand_for_port(ir::port &p, bool constant_fold = true);
	int vreg_for_port(ir::port &p) const { return port_to_vreg_.at(&p); }

    void materialise(const ir::node *n);
    void materialise_read_reg(const ir::read_reg_node &n);
    void materialise_write_reg(const ir::write_reg_node &n);
    void materialise_read_mem(const ir::read_mem_node &n);
    void materialise_write_mem(const ir::write_mem_node &n);
    void materialise_constant(const ir::constant_node &n);
    void materialise_binary_arith(const ir::binary_arith_node &n);
};
} // namespace arancini::output::dynamic::arm64

