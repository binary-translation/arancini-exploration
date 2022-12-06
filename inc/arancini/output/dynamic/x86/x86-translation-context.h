#pragma once

#include <arancini/ir/node.h>
#include <arancini/ir/port.h>
#include <arancini/output/dynamic/translation-context.h>
#include <arancini/output/dynamic/x86/machine-code-builder.h>
#include <map>
#include <set>

namespace arancini::output::dynamic::x86 {
class x86_translation_context : public translation_context {
public:
	x86_translation_context(machine_code_writer &writer)
		: translation_context(writer)
		, next_vreg_(0)
	{
	}

	virtual void begin_block() override;
	virtual void begin_instruction(off_t address, const std::string& disasm) override;
	virtual void end_instruction() override;
	virtual void end_block() override;
	virtual void lower(ir::node *n) override;

private:
	machine_code_builder builder_;
	std::set<ir::node *> materialised_nodes_;
	std::map<ir::port *, int> port_to_vreg_;
	int next_vreg_;

	int alloc_vreg() { return next_vreg_++; }

	int alloc_vreg_for_port(ir::port &p)
	{
		int v = alloc_vreg();
		port_to_vreg_[&p] = v;
		return v;
	}

	operand operand_for_port(ir::port &p);
	operand vreg_operand_for_port(ir::port &p);
	int vreg_for_port(ir::port &p) const { return port_to_vreg_.at(&p); }

	void materialise(ir::node *n);
	void materialise_read_reg(ir::read_reg_node *n);
	void materialise_write_reg(ir::write_reg_node *n);
	void materialise_binary_arith(ir::binary_arith_node *n);
	void materialise_cast(ir::cast_node *n);
	void materialise_constant(ir::constant_node *n);
	void materialise_read_mem(ir::read_mem_node *n);
	void materialise_write_mem(ir::write_mem_node *n);
	void materialise_read_pc(ir::read_pc_node *n);
	void materialise_write_pc(ir::write_pc_node *n);
};
} // namespace arancini::output::dynamic::x86
