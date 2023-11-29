#pragma once

#include "arancini/ir/value-type.h"
#include <arancini/ir/node.h>
#include <arancini/ir/chunk.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/opt.h>
#include <arancini/ir/default-visitor.h>
#include <arancini/runtime/exec/x86/x86-cpu-state.h>

#include <cstring>
#include <iostream>
#include <llvm/IR/Type.h>
#include <set>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace arancini::ir {
	class llvm_ret_visitor : public default_visitor {
		public:
			virtual void visit_chunk(chunk &c) override;

			virtual void visit_write_reg_node(write_reg_node & n) override;
			//virtual void visit_unary_atomic_node(unary_atomic_node &n) override;
			//virtual void visit_binary_atomic_node(binary_atomic_node &n) override;
			//virtual void visit_ternary_atomic_node(ternary_atomic_node &n) override;

			virtual void visit_write_pc_node(write_pc_node & n) override;
				
			void resolve_waiting();

			auto get_type(unsigned long fn) { return types_.at(fn); };

			void debug_print();
		private:
			unsigned long current_chunk_;
			unsigned long current_pkt_;
			std::unordered_map<unsigned long, unsigned long> waiting_;	// key is waiting for elem to be resolved
			std::unordered_map<unsigned long, std::set<reg_offsets>> types_;
			std::unordered_set<reg_offsets> current_possible_;
			const std::unordered_set<reg_offsets> possible_rets = {
				reg_offsets::RAX, reg_offsets::RDX,
				reg_offsets::ZMM0, reg_offsets::ZMM1
			};
	};
	
	class llvm_arg_visitor : public default_visitor {
		public:
			//llvm_arg_visitor(void) : {}

			virtual void visit_chunk(chunk &c) override;
			//virtual void visit_unary_atomic_node(unary_atomic_node &n) override;
			//virtual void visit_binary_atomic_node(binary_atomic_node &n) override;
			//virtual void visit_ternary_atomic_node(ternary_atomic_node &n) override;

			virtual void visit_write_pc_node(write_pc_node & n) override;
			
			virtual void visit_read_reg_node(read_reg_node & n) override;
			
			void resolve_waiting();

			auto get_type(unsigned long fn) { return types_.at(fn); };

			void debug_print();
		private:
			unsigned long current_chunk_;
			unsigned long current_pkt_;
			std::unordered_map<unsigned long, unsigned long> waiting_;	// key is waiting for elem to be resolved
			std::unordered_map<unsigned long, std::set<reg_offsets>> types_;
			std::unordered_set<reg_offsets> current_possible_;
			const std::unordered_set<reg_offsets> possible_args = {
				reg_offsets::RDI, reg_offsets::RSI,
				reg_offsets::RDX, reg_offsets::RCX,
				reg_offsets::R8, reg_offsets::R9,
				reg_offsets::ZMM0, reg_offsets::ZMM1,
				reg_offsets::ZMM2, reg_offsets::ZMM3,
				reg_offsets::ZMM4, reg_offsets::ZMM7,
				reg_offsets::ZMM6, reg_offsets::ZMM7
			};
	};
}
