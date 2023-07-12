#pragma once

#include <string>
#include <vector>

#include <arancini/ir/node.h>
#include <arancini/ir/visitor.h>

extern "C" {
#include <xed/xed-interface.h>
}

namespace arancini::ir {
class packet {
public:
	packet(off_t address, const std::string &disassembly = "")
		: address_(address)
		, disasm_(disassembly)
	{
	}

	local_var *alloc_local(const value_type &type)
	{
		auto lcl = new local_var(type);
		locals_.push_back(lcl);

		return lcl;
	}

	void append_action(action_node *node) { actions_.push_back(node); }

	const std::vector<action_node *> &actions() const { return actions_; }

  void set_actions(std::vector<action_node *> actions) { actions_ = actions;}

	void accept(visitor &v) { v.visit_packet(*this); }

	off_t address() const { return address_; }

	bool updates_pc() const
	{
		for (action_node *a : actions_) {
			if (a->updates_pc()) {
				return true;
			}
		}

		return false;
	}

	const std::string &disassembly() const { return disasm_; }

private:
	off_t address_;
	std::string disasm_;
	std::vector<local_var *> locals_;
	std::vector<action_node *> actions_;
};
} // namespace arancini::ir
