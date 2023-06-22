#pragma once

#include <arancini/ir/node.h>
#include <arancini/ir/chunk.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/default-visitor.h>
#include <cstring>
#include <iostream>
#include <set>
#include <algorithm>

namespace arancini::ir {
  class deadflags_opt_visitor : public default_visitor {
  public:
    deadflags_opt_visitor(void) {
      nr_flags_total_ = nr_flags_opt_total_ = 0;
    }

    ~deadflags_opt_visitor(void) {
      std::cout << "Dead flags opt pass stats: optimised " << nr_flags_opt_total_ << "/" << nr_flags_total_ << " (" << 100 * nr_flags_opt_total_ / nr_flags_total_ << "%)" << std::endl;
    }

	  void visit_chunk(chunk & c) {
		  nr_flags_ = nr_flags_opt_ = 0;
		  last_se_packet_ = nullptr;
      live_flags_.clear();

		  auto packets = c.packets();
		  for (auto p = packets.rbegin(); p != packets.rend(); ++p) {
			  (*p)->accept(*this);
		  }
		  //std::cout << "Dead flags opt pass @ " << c.address() << ": optimised out " << nr_flags_opt_ << "/" << nr_flags_ << " flags" << std::endl;
		  nr_flags_total_ += nr_flags_;
		  nr_flags_opt_total_ += nr_flags_opt_;
	  }

	  void visit_packet(packet & p) {
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
			  // unlink the write_reg node from the operation producing the flag change
			  write_reg_node *wr_node = (write_reg_node *)n;
			  node *op = wr_node->value().owner();
			  switch (op->kind()) {
			  case node_kinds::unary_arith:
			  case node_kinds::binary_arith:
			  case node_kinds::ternary_arith: {
				  if (!strncmp(wr_node->regname(), "ZF", 2)) {
					  ((arith_node *)op)->zero().remove_target(wr_node);
				  } else if (!strncmp(wr_node->regname(), "CF", 2)) {
					  ((arith_node *)op)->carry().remove_target(wr_node);
				  } else if (!strncmp(wr_node->regname(), "OF", 2)) {
					  ((arith_node *)op)->overflow().remove_target(wr_node);
				  } else if (!strncmp(wr_node->regname(), "SF", 2)) {
					  ((arith_node *)op)->negative().remove_target(wr_node);
				  } else {
					  throw std::runtime_error("unsupported flag for flag optimisation");
				  }
				  break;
			  }
			  case node_kinds::unary_atomic:
			  case node_kinds::binary_atomic:
			  case node_kinds::ternary_atomic: {
				  if (!strncmp(wr_node->regname(), "ZF", 2)) {
					  ((atomic_node *)op)->zero().remove_target(wr_node);
				  } else if (!strncmp(wr_node->regname(), "CF", 2)) {
					  ((atomic_node *)op)->carry().remove_target(wr_node);
				  } else if (!strncmp(wr_node->regname(), "OF", 2)) {
					  ((atomic_node *)op)->overflow().remove_target(wr_node);
				  } else if (!strncmp(wr_node->regname(), "SF", 2)) {
					  ((atomic_node *)op)->negative().remove_target(wr_node);
				  } else {
					  throw std::runtime_error("unsupported flag for flag optimisation");
				  }
          break;
			  }
			  default:
				  break;
			  }

			  // remove node from the action_node list
			  end = std::remove(begin, end, n);
			  nr_flags_opt_++;
		  }
		  actions.erase(end, actions.end());
		  p.set_actions(actions);
		  delete_.clear();
	  }

	  void visit_read_reg_node(read_reg_node & n)
	  {
		  if (!flag_regs_names_.count(n.regname()))
			  return;

		  live_flags_.insert(n.regidx());
	  }

	  void visit_write_reg_node(write_reg_node & n)
	  {
		  if (!flag_regs_names_.count(n.regname()))
			  return;

		  nr_flags_++;
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
    std::set<std::string> flag_regs_names_ = { "ZF", "CF", "OF", "SF", "PF", "AF" };
    unsigned int nr_flags_, nr_flags_opt_, nr_flags_total_, nr_flags_opt_total_;
  };
}
