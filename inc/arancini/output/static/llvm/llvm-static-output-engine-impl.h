#pragma once
#include "arancini/output/static/llvm/llvm-static-visitor.h"
#include <arancini/ir/node.h>
#include <llvm/ADT/ArrayRef.h>
#include <arancini/ir/opt.h>
#include <llvm/IR/BasicBlock.h>
#include <map>
#include <memory>
#include <unordered_map>

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>

#include <llvm/MC/TargetRegistry.h>

#include <llvm/Passes/PassBuilder.h>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>

#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>

namespace arancini::ir {
class chunk;
class node;
class port;
class packet;
class label_node;
class llvm_ret_visitor;
class llvm_arg_visitor;
} // namespace arancini::ir

namespace arancini::output {
class static_output_personality;
}

namespace arancini::output::o_static::llvm {
class llvm_static_output_engine;

class llvm_static_output_engine_impl {
public:
	llvm_static_output_engine_impl(const llvm_static_output_engine &e, const std::vector<std::pair<unsigned long, std::string>> &extern_fns, const std::vector<std::shared_ptr<ir::chunk>> &chunks);

	void generate();

	unsigned long fixed_branches;
private:
	const llvm_static_output_engine &e_;
	const std::vector<std::pair<unsigned long, std::string>> &extern_fns_;
	const std::vector<std::shared_ptr<ir::chunk>> &chunks_;
	std::unique_ptr<::llvm::LLVMContext> llvm_context_;
	std::unique_ptr<::llvm::Module> module_;
	std::shared_ptr<std::map<unsigned long, ::llvm::Function *>> fns_ = std::make_shared<std::map<unsigned long, ::llvm::Function *>>();
	std::vector<::llvm::Constant *> func_map_;
	bool in_br;

	struct {
		::llvm::Type *vd;
		::llvm::IntegerType *i1;
		::llvm::IntegerType *i8;
		::llvm::IntegerType *i16;
		::llvm::IntegerType *i32;
		::llvm::IntegerType *i64;
		::llvm::IntegerType *i128;
		::llvm::IntegerType *i256;
		::llvm::IntegerType *i512;
		::llvm::Type *f32;
		::llvm::Type *f64;
		::llvm::Type *f80;
		::llvm::Type *f128;
		::llvm::StructType *cpu_state;
		::llvm::PointerType *cpu_state_ptr;
		::llvm::FunctionType *main_fn;
		::llvm::FunctionType *loop_fn;
		::llvm::FunctionType *chunk_fn;
		::llvm::FunctionType *init_dbt;
		::llvm::FunctionType *dbt_invoke;
		::llvm::FunctionType *internal_call_handler;
		::llvm::FunctionType *finalize;
		::llvm::FunctionType *clk_fn;
		::llvm::FunctionType *register_static_fn;
		::llvm::FunctionType *lookup_static_fn;

		::llvm::IntegerType *integer(unsigned width) {
			switch(width) {
			case 1: return i1;
			case 8: return i8;
			case 16: return i16;
			case 32: return i32;
			case 64: return i64;
			case 128: return i128;
			default:
			throw std::runtime_error("unsupported integer width");
			}
		}
	} types;

	::llvm::MDNode *guest_mem_alias_scope_;
	::llvm::MDNode *reg_file_alias_scope_;

	std::map<ir::port *, ::llvm::Value *> node_ports_to_llvm_values_;
	std::map<ir::label_node *, ::llvm::BasicBlock *> label_nodes_to_llvm_blocks_;
	std::unordered_map<const ir::local_var *, ::llvm::Value *> local_var_to_llvm_addr_;
	std::unordered_map<reg_offsets, ::llvm::Value*> reg_to_alloca_;

	void build();
	void initialise_types();
	void create_main_function(::llvm::Function *loop_fn);
	void optimise();
	void compile();
	void lower_chunks(::llvm::Function *main_loop_fn);
	void lower_chunk(::llvm::IRBuilder<> *builder, ::llvm::Function *main_loop_fn, std::shared_ptr<ir::chunk> chunk);
	void lower_static_fn_lookup(::llvm::IRBuilder<> &builder, ::llvm::BasicBlock *contblock, ::llvm::Value *guestAddr);
	::llvm::Value *lower_node(::llvm::IRBuilder<::llvm::ConstantFolder, ::llvm::IRBuilderDefaultInserter> &builder, ::llvm::Argument *start_arg,
		std::shared_ptr<ir::packet> pkt, ir::node *a);
	::llvm::Value *lower_port(::llvm::IRBuilder<::llvm::ConstantFolder, ::llvm::IRBuilderDefaultInserter> &builder, ::llvm::Argument *start_arg,
		std::shared_ptr<ir::packet> pkt, ir::port &p);
	::llvm::Value *materialise_port(::llvm::IRBuilder<::llvm::ConstantFolder, ::llvm::IRBuilderDefaultInserter> &builder, ::llvm::Argument *start_arg,
		std::shared_ptr<ir::packet> pkt, ir::port &p);
	void init_regs(::llvm::IRBuilder<> &builder);
	void save_all_regs(::llvm::IRBuilder<> &builder, ::llvm::Argument *state_arg);
	void restore_all_regs(::llvm::IRBuilder<> &builder, ::llvm::Argument *state_arg);
	void save_callee_regs(::llvm::IRBuilder<> &builder, ::llvm::Argument *state_arg, bool with_args=true);
	void restore_callee_regs(::llvm::IRBuilder<> &builder, ::llvm::Argument *state_arg, bool with_rets=true);
	// passes
	::llvm::Function *get_static_fn(std::shared_ptr<ir::packet> pkt);
	::llvm::Instruction *create_static_condbr(::llvm::IRBuilder<> *builder, std::shared_ptr<ir::packet> pkt, std::map<unsigned long, ::llvm::BasicBlock *> *blocks, ::llvm::BasicBlock *mid);
	::llvm::FunctionType *get_fn_type();
	std::vector<::llvm::Value *> load_args(::llvm::IRBuilder<> *builder, ::llvm::Argument *state_arg);
	void unwrap_ret(::llvm::IRBuilder<> *builder, ::llvm::Value *value, ::llvm::Argument *state_arg);
	std::vector<::llvm::Value*> wrap_ret(::llvm::IRBuilder<> *builder, ::llvm::Argument *state_arg);
	void create_function_decls();
	void create_static_functions();
	
	::llvm::Value *createLoadFromCPU(::llvm::IRBuilder<> &builder, ::llvm::Argument *state_arg, unsigned long reg_idx);
	void createStoreToCPU(::llvm::IRBuilder<> &builder, ::llvm::Argument *state_arg, unsigned int ret_idx, ::llvm::Value *ret, unsigned long reg_idx);
	void debug_dump();
};
} // namespace arancini::output::o_static::llvm
