#pragma once
#include <arancini/ir/node.h>

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
} // namespace arancini::ir

namespace arancini::output {
class static_output_personality;
}

const std::string ARANCINI_FUNCTION_TYPE = "arancini_func_ty";
const std::string ARANCINI_STATIC_FUNCTION = "static_func";

namespace arancini::output::o_static::llvm {
class llvm_static_output_engine;

class llvm_static_output_engine_impl {
public:
	llvm_static_output_engine_impl(const llvm_static_output_engine &e, const std::vector<std::shared_ptr<ir::chunk>> &chunks);

	void generate();

	unsigned long fixed_branches;
private:
	const llvm_static_output_engine &e_;
	const std::vector<std::shared_ptr<ir::chunk>> &chunks_;
	std::unique_ptr<::llvm::LLVMContext> llvm_context_;
	std::unique_ptr<::llvm::Module> module_;
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
		::llvm::StructType *cpu_state;
		::llvm::PointerType *cpu_state_ptr;
		::llvm::FunctionType *main_fn;
		::llvm::FunctionType *loop_fn;
		::llvm::FunctionType *init_dbt;
		::llvm::FunctionType *dbt_invoke;
		::llvm::FunctionType *internal_call_handler;
		::llvm::FunctionType *finalize;

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

	void build();
	void initialise_types();
	void create_main_function(::llvm::Function *loop_fn);
	void optimise();
	void compile();
	void lower_chunks(::llvm::SwitchInst *pcswitch, ::llvm::BasicBlock *contblock);
	void lower_chunk(::llvm::IRBuilder<> *builder, ::llvm::BasicBlock *contblock, std::shared_ptr<ir::chunk> chunk, std::shared_ptr<std::map<unsigned long, ::llvm::Function *>> fns);
	::llvm::Value *lower_node(::llvm::IRBuilder<::llvm::ConstantFolder, ::llvm::IRBuilderDefaultInserter> &builder, ::llvm::Argument *start_arg,
		std::shared_ptr<ir::packet> pkt, ir::node *a);
	::llvm::Value *lower_port(::llvm::IRBuilder<::llvm::ConstantFolder, ::llvm::IRBuilderDefaultInserter> &builder, ::llvm::Argument *start_arg,
		std::shared_ptr<ir::packet> pkt, ir::port &p);
	::llvm::Value *materialise_port(::llvm::IRBuilder<::llvm::ConstantFolder, ::llvm::IRBuilderDefaultInserter> &builder, ::llvm::Argument *start_arg,
		std::shared_ptr<ir::packet> pkt, ir::port &p);
	::llvm::Function *get_static_fn(std::shared_ptr<ir::packet> pkt, std::shared_ptr<std::map<unsigned long, ::llvm::Function *>>);
	::llvm::Instruction *create_static_condbr(::llvm::IRBuilder<> *builder, std::shared_ptr<ir::packet> pkt, std::map<unsigned long, ::llvm::BasicBlock *> *blocks, ::llvm::BasicBlock *mid);
};
} // namespace arancini::output::o_static::llvm
