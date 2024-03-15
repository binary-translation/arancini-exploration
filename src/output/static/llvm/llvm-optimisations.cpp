#include "arancini/ir/default-visitor.h"
#include "arancini/ir/node.h"
#include "arancini/ir/opt.h"
#include "arancini/ir/port.h"
#include "arancini/ir/value-type.h"
#include <arancini/output/static/llvm/llvm-static-output-engine-impl.h>
#include <arancini/output/static/llvm/llvm-static-output-engine.h>
#include <arancini/output/static/llvm/llvm-static-visitor.h>
#include <iterator>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <map>
#include <ostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace arancini::output::o_static::llvm;
using namespace arancini::ir;
using namespace ::llvm;

static const std::unordered_map<reg_offsets, char*>regnames = {
#define DEFREG(ctype, ltype, name) { reg_offsets::name, "" #name},
#include <arancini/input/x86/reg.def>
#undef DEFREG
};

static std::ostream& print(std::ostream& out, std::set<reg_offsets>& regs) {
	for (auto r : regs) {
		out << regnames.at(r) << ", ";
	}
	out << std::endl;
	return out;
}

void llvm_ret_visitor::debug_print() {
	for (auto it = types_.begin(); it != types_.end(); it++) {
		std::cout << std::hex << it->first << ": ";
		print(std::cout, it->second);
	}
}

void llvm_arg_visitor::debug_print() {
	for (auto it = types_.begin(); it != types_.end(); it++) {
		std::cout << std::hex << it->first << ": ";
		print(std::cout, it->second);
	}
}

void llvm_ret_visitor::visit_chunk(chunk &c) {
	current_chunk_ = c.packets()[0]->address();
	types_[current_chunk_] = {};
	current_possible_ = possible_rets;
	
	auto pkts = c.packets();
	for (auto rit = pkts.crbegin(); rit != pkts.crend(); rit++) {
		current_pkt_ = (*rit)->address();
		for (auto an = (*rit)->actions().cbegin(); an != (*rit)->actions().cend(); an++) {
			if (current_possible_.empty()) return;
			(*an)->accept(*this);
		}
	}
}

void llvm_ret_visitor::visit_write_reg_node(write_reg_node &n) {
	auto reg = (reg_offsets)n.regoff();
	for ( auto it = current_possible_.begin(); it != current_possible_.end(); it++ ) {
		if ( *it == reg ) {
			// no need to check again
			current_possible_.erase(it);
			types_[current_chunk_].insert(reg);
			break;
		}
	}
	return default_visitor::visit_write_reg_node(n);
}

/*
void llvm_ret_visitor::visit_unary_atomic_node(unary_atomic_node &n) {
	// TODO
}

void llvm_ret_visitor::visit_binary_atomic_node(binary_atomic_node &n) {
	// TODO
}

void llvm_ret_visitor::visit_ternary_atomic_node(ternary_atomic_node &n) {
	// TODO
}
*/

void llvm_ret_visitor::visit_write_pc_node(write_pc_node &n) {
	if (n.updates_pc() == br_type::call) {
		// add all returns from the callee
		auto callee = n.const_target()+current_pkt_;
		if (callee == current_pkt_) {
			// here we have to assume everything?
			for (auto r : possible_rets) {
				types_[current_chunk_].insert(r);
			}
			current_possible_.clear();
			return;
		}
		if (types_.find(callee) != types_.end()) {
			for (auto r : types_.at(callee)) {
				if (auto it = current_possible_.find(r); it != current_possible_.end()) {
					current_possible_.erase(it);
					types_[current_chunk_].insert(r);
				}
			}
			return;
		}
		waiting_[current_chunk_] = callee;
		return;
	}
	if (n.updates_pc() == br_type::sys) {
		for (auto it = current_possible_.begin(); it != current_possible_.end(); it++) {
			if (*it == reg_offsets::RAX) {
				types_[current_chunk_].insert(*it);
				current_possible_.erase(it);
			}
		}
		return;
	}
	default_visitor::visit_write_pc_node(n);
}

void llvm_ret_visitor::resolve_waiting() {

	if (waiting_.empty()) return;

	std::set<unsigned long> done;
	std::vector<unsigned long> todo;
	for (auto it = waiting_.begin(); it != waiting_.end(); it++) todo.push_back(it->first);
	while (!todo.empty()) {
		auto waiter = todo.back();
		if (done.find(waiter) != done.end()) { todo.pop_back(); continue; }

		auto awaited = waiting_.at(waiter);
		if (types_.find(awaited) != types_.end()) {
			for(auto r : types_.at(awaited)) {
				types_[waiter].insert(r);
			}
			done.insert(waiter);
			todo.pop_back();
			continue;
		}
		
		todo.push_back(awaited);
	}
}

void llvm_arg_visitor::visit_chunk(chunk &c) {
	current_chunk_ = c.packets()[0]->address();
	types_[current_chunk_] = {};
	current_possible_ = possible_args;

	auto pkts = c.packets();
	for (auto rit = pkts.cbegin(); rit != pkts.cend(); rit++) {
		current_pkt_ = (*rit)->address();
		for (auto an = (*rit)->actions().cbegin(); an != (*rit)->actions().cend(); an++) {
			if (current_possible_.empty()) return;
			(*an)->accept(*this);
		}
	}
}

void llvm_arg_visitor::visit_read_reg_node(read_reg_node &n) {
	auto reg = (reg_offsets)n.regoff();	
	for ( auto it = current_possible_.begin(); it != current_possible_.end(); it++ ) {
		if ( *it == reg ) {
			// no need to check again
			current_possible_.erase(it);
			types_[current_chunk_].insert(*it);
			break;
		}
	}
}

/*
void llvm_arg_visitor::visit_unary_atomic_node(unary_atomic_node &n) {
	// TODO
}

void llvm_arg_visitor::visit_binary_atomic_node(binary_atomic_node &n) {
	// TODO
}

void llvm_arg_visitor::visit_ternary_atomic_node(ternary_atomic_node &n) {
	// TODO
}
*/

void llvm_arg_visitor::visit_write_pc_node(write_pc_node &n) {
	default_visitor::visit_write_pc_node(n);
	if (n.updates_pc() == br_type::call) {
		// add all argument from the callee
		auto callee = n.const_target()+current_pkt_;
		if (callee == current_pkt_) {
			// here we have to assume everything?
			for (auto r : possible_args) {
				types_[current_chunk_].insert(r);
			}
			current_possible_.clear();
			return;
		}
		if (types_.find(callee) != types_.end()) {
			for (auto r : types_.at(callee)) {
				if (auto it = current_possible_.find(r); it != current_possible_.end()) {
					current_possible_.erase(it);
					types_[current_chunk_].insert(r);
				}
			}
			return;
		}
		waiting_[current_chunk_] = callee;
		return;
	}
	if (n.updates_pc() == br_type::sys) {
		// this is a syscall, so rdi, rsi, rdx, r8, r9 might be used
		for (auto it = current_possible_.begin(); it != current_possible_.end(); it++) {
			if ((*it == reg_offsets::RDI) ||
				(*it == reg_offsets::RSI) ||
				(*it == reg_offsets::RDX) ||
				(*it == reg_offsets::R8) ||
				(*it == reg_offsets::R9)) {
				types_[current_chunk_].insert(*it);
				current_possible_.erase(it);
			}
		}
		return;
	}
}

void llvm_arg_visitor::resolve_waiting() {

	if (waiting_.empty()) return;


	std::set<unsigned long> done;
	std::vector<unsigned long> todo;
	for (auto it = waiting_.begin(); it != waiting_.end(); it++) todo.push_back(it->first);
	while (!todo.empty()) {
		auto waiter = todo.back();
		if (done.find(waiter) != done.end()) { todo.pop_back(); continue; }

		auto awaited = waiting_.at(waiter);
		if (types_.find(awaited) != types_.end()) {
			for(auto r : types_.at(awaited)) {
				types_[waiter].insert(r);
			}
			done.insert(waiter);
			todo.pop_back();
			continue;
		}
		
		todo.push_back(awaited);
	}
}

Instruction *llvm_static_output_engine_impl::create_static_br(
	IRBuilder<> *builder, std::shared_ptr<packet> pkt, std::map<unsigned long, BasicBlock *> *blocks, BasicBlock *mid)
{
	auto it = builder->GetInsertPoint();
	if ((--it)->getOpcode() != Instruction::Store)
		return nullptr;

	if (it->getOperand(1)->getName()!="regPC") {
		return nullptr;
	}

	auto addr = dyn_cast<ConstantInt>(it->getOperand(0));

	BasicBlock *follow_block = mid;
	std::map<unsigned long, BasicBlock *>::iterator bb_it;

	// Add can constant fold on creation
	if (addr) {
		bb_it = blocks->find(addr->getZExtValue());
		follow_block = bb_it != blocks->end() ? bb_it->second : mid;
	} else {
		auto addr1 = dyn_cast<ConstantExpr>(it->getOperand(0));
		ConstantInt *pc_val = nullptr;
		ConstantInt *offs = nullptr;
		if (addr1 && addr1->getOpcode() == Instruction::Add) {
			auto pc = dyn_cast<ConstantExpr>(addr1->getOperand(0));
			if (pc && pc->getOpcode() == Instruction::PtrToInt) {
				pc = dyn_cast<ConstantExpr>(pc->getOperand(0));
				if (pc && pc->getOpcode() == Instruction::GetElementPtr) {
					pc_val = dyn_cast<ConstantInt>(pc->getOperand(1));
				}
			}
			offs = dyn_cast<ConstantInt>(addr1->getOperand(1));
		}
		if (offs && pc_val) {
			bb_it = blocks->find(offs->getZExtValue() + pc_val->getZExtValue());
			follow_block = bb_it != blocks->end() ? bb_it->second : mid;
		}
	}

	if (follow_block != mid)
		fixed_branches++;
	return builder->CreateBr(follow_block);
};

Instruction *llvm_static_output_engine_impl::create_static_condbr(
	IRBuilder<> *builder, std::shared_ptr<packet> pkt, std::map<unsigned long, BasicBlock *> *blocks, BasicBlock *mid)
{
	auto it = builder->GetInsertPoint();
	if ((--it)->getOpcode() != Instruction::Store)
		return nullptr;
	
	if ((--it)->getOpcode() != Instruction::Select)
		return nullptr;

	auto cond = it->getOperand(0);
	auto true_addr = dyn_cast<ConstantInt>(it->getOperand(1));
	auto false_addr = dyn_cast<ConstantInt>(it->getOperand(2));

	BasicBlock *true_block = mid;
	BasicBlock *false_block = mid;
	std::map<unsigned long, BasicBlock *>::iterator bb_it;

	// Add can constant fold on creation
	if (true_addr) {
		bb_it = blocks->find(true_addr->getZExtValue());
		true_block = bb_it != blocks->end() ? bb_it->second : mid;
	} else {
		auto true_addr1 = dyn_cast<ConstantExpr>(it->getOperand(1));
		ConstantInt *pc_val = nullptr;
		ConstantInt *offs = nullptr;
		if (true_addr1 && true_addr1->getOpcode() == Instruction::Add) {

			auto pc = dyn_cast<ConstantExpr>(true_addr1->getOperand(0));
			if (pc && pc->getOpcode() == Instruction::PtrToInt) {
				pc = dyn_cast<ConstantExpr>(pc->getOperand(0));
				if (pc && pc->getOpcode() == Instruction::GetElementPtr) {
					pc_val = dyn_cast<ConstantInt>(pc->getOperand(1));
				}
			}
			offs = dyn_cast<ConstantInt>(true_addr1->getOperand(1));
		}
		if (offs && pc_val) {
			bb_it = blocks->find(offs->getZExtValue() + pc_val->getZExtValue());
			true_block = bb_it != blocks->end() ? bb_it->second : mid;
		}
	}

	if (false_addr) {
		bb_it = blocks->find(false_addr->getZExtValue());
		false_block = bb_it != blocks->end() ? bb_it->second : mid;
	}
	else {
		auto false_addr1 = dyn_cast<ConstantExpr>(it->getOperand(2));
		ConstantInt *pc_val = nullptr;
		ConstantInt* offs = nullptr;
		if (false_addr1 && false_addr1->getOpcode() == Instruction::Add) {

			auto pc = dyn_cast<ConstantExpr>(false_addr1->getOperand(0));
			if (pc && pc->getOpcode() == Instruction::PtrToInt) {
				pc = dyn_cast<ConstantExpr>(pc->getOperand(0));
				if (pc && pc->getOpcode() == Instruction::GetElementPtr) {
					pc_val = dyn_cast<ConstantInt>(pc->getOperand(1));
				}
			}
			offs = dyn_cast<ConstantInt>(false_addr1->getOperand(1));
		}
		if (offs && pc_val) {
			bb_it = blocks->find(offs->getZExtValue() + pc_val->getZExtValue());
			false_block = bb_it != blocks->end() ? bb_it->second : mid;
		}
	}
	if (true_block != mid && false_block != mid)
		fixed_branches++;
	return builder->CreateCondBr(cond, true_block, false_block);
};
