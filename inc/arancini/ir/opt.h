#pragma once

#include <arancini/ir/chunk.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/default-visitor.h>
#include <iostream>
#include <set>
#include <algorithm>

namespace arancini::ir {
class deadflags_opt_visitor : public default_visitor {
public:
  deadflags_opt_visitor(void) {
    std::cerr << "flag_regs_names_ = [";
    for (auto s : flag_regs_names_) {
      std::cerr << s << ", ";
    }
    std::cerr << "]" << std::endl;
  }

	void visit_chunk(chunk &c) {
    last_se_packet_ = nullptr;
    auto packets = c.packets();
    for (auto p = packets.rbegin(); p != packets.rend(); ++p) {
      (*p)->accept(*this);
    }
	}

  void visit_packet(packet &p) {
    current_packet_ = &p;
    auto actions = p.actions();
    for (auto a = actions.rbegin(); a != actions.rend(); ++a) {
      (*a)->accept(*this);
    }

    // delete all write reg nodes marked for deletion
    if (!delete_.size())
      return;
    auto begin = actions.begin(), end = actions.end();
    for (auto n : delete_) {
      end = std::remove(begin, end, n);
    }
    actions.erase(end, actions.end());
    p.set_actions(actions);
    delete_.clear();
  }

  void visit_read_reg_node(read_reg_node &n) {
    if (!flag_regs_names_.count(n.regname()))
      return;

    live_flags_.insert(n.regidx());
  }

  void visit_write_reg_node(write_reg_node &n) {
    if (!flag_regs_names_.count(n.regname()))
      return;

    // if we are in the last packet modifying flags in the chunk, we cannot optimise out, even if the flag is not live,
    // since it can be used in a following chunk, e.g. basic block
    if (!last_se_packet_)
      last_se_packet_ = current_packet_;
    if (last_se_packet_ == current_packet_)
      return;

    // if this flag is live, i.e. is read later, we keep the write reg node and mark it dead. Else, we delete it.
    if (live_flags_.count(n.regidx())) {
      live_flags_.erase(n.regidx());
    } else {
      delete_.push_back((action_node *)&n);
    }
  }

private:
  std::vector<action_node *> delete_;
  std::set<unsigned long> live_flags_;
  packet *current_packet_, *last_se_packet_; // last packet with side effects on flags in the current chunk
  std::set<std::string> flag_regs_names_ = { "ZF", "CF", "OF", "SF", "PF", "DF" };
};
}
