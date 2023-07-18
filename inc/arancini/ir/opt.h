#pragma once

#include <arancini/ir/node.h>
#include <arancini/ir/chunk.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/default-visitor.h>
#include <arancini/runtime/exec/x86/x86-cpu-state.h>

#include <cstring>
#include <iostream>
#include <set>
#include <algorithm>
#include <unordered_map>

#define X86_OFFSET_OF(reg) __builtin_offsetof(struct arancini::runtime::exec::x86::x86_cpu_state, reg)
enum class reg_offsets : unsigned long {
#define DEFREG(ctype, ltype, name) name = X86_OFFSET_OF(name),
#include <arancini/input/x86/reg.def>
#undef DEFREG
};

namespace arancini::ir {
  class deadflags_opt_visitor : public default_visitor {
  public:
    deadflags_opt_visitor(void) {
      nr_flags_total_ = nr_flags_opt_total_ = 0;
    }

	~deadflags_opt_visitor(void)
	{
		if (nr_flags_total_ != 0) {
			std::cout << "Dead flags opt pass stats: optimised " << nr_flags_opt_total_ << "/" << nr_flags_total_ << " ("
					  << 100 * nr_flags_opt_total_ / nr_flags_total_ << "%)" << std::endl;
		}
	}

	void visit_chunk(chunk &c)
	{
		nr_flags_ = nr_flags_opt_ = 0;
		last_se_packets_.clear();
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
		if (!delete_.empty()) {
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
		live_flags_.insert(new_live_flags.begin(), new_live_flags.end());
		new_live_flags.clear();
	}

	  void visit_read_reg_node(read_reg_node & n)
	  {
		if (!flag_regs_offsets_.count(static_cast<reg_offsets>(n.regoff())))
			  return;

		// If the flag register is also written in this packet, deferring insertion ensures later writes in the same packet don't "satisfy" the read
		new_live_flags.insert(n.regoff());
	}

	void visit_write_reg_node(write_reg_node &n)
	{
		if (flag_regs_offsets_.count(static_cast<reg_offsets>(n.regoff())) != 0) {
			nr_flags_++;
			// if we are in the last packet modifying flags in the chunk, we cannot optimise out, even if the flag is not live,
			// since it can be used in a following chunk, e.g. basic block
			if (!last_se_packets_.count(n.regoff())) {
				last_se_packets_[n.regoff()] = current_packet_;
				live_flags_.erase(n.regoff());
			} else {
				// if this flag is live, i.e. is read later, we keep the write reg node and mark it dead. Else, we delete it.
				if (live_flags_.count(n.regoff())) {
					live_flags_.erase(n.regoff());
				} else {
					delete_.push_back((action_node *)&n);
				}
			}
		}
		default_visitor::visit_write_reg_node(n);
	}

private:
	std::vector<action_node *> delete_;
	std::set<unsigned long> live_flags_, new_live_flags;
	packet *current_packet_;
	std::unordered_map<unsigned long, packet *> last_se_packets_; // last packet with side effects on each flag in the current chunk
	std::set<enum reg_offsets> flag_regs_offsets_
		= { reg_offsets::ZF, reg_offsets::CF, reg_offsets::OF, reg_offsets::SF, reg_offsets::PF /*, reg_offsets::AF FIXME not included in reg.def */ };
	unsigned int nr_flags_, nr_flags_opt_, nr_flags_total_, nr_flags_opt_total_;
  };
}
