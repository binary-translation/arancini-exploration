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

	void append_action(action_node *node) { actions_.push_back(node); }

	const std::vector<action_node *> &actions() const { return actions_; }

	bool accept(visitor &v)
	{
		if (!v.visit_packet_start(*this)) {
			return false;
		}

		for (auto n : actions_) {
			n->accept(v);
		}

		return v.visit_packet_end(*this);
	}

	off_t address() const { return address_; }

	bool updates_pc() const
	{
		for (action_node *a : actions_) {
			if (a->kind() == node_kinds::write_pc) {
				return true;
			}
		}

		return false;
	}

	const std::string &disassembly() const { return disasm_; }

private:
	off_t address_;
	std::string disasm_;
	std::vector<action_node *> actions_;
};
} // namespace arancini::ir
