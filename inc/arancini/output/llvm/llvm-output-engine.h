#pragma once

#include <arancini/output/output-engine.h>
#include <memory>

namespace llvm {
class LLVMContext;
class Argument;
class Module;
class MDNode;
class FunctionType;
class Type;
class IntegerType;
class Function;
class PointerType;
class StructType;
class SwitchInst;
class Value;
class BasicBlock;
class ConstantFolder;
class IRBuilderDefaultInserter;
template <typename FolderTy, typename InserterTy> class IRBuilder;
} // namespace llvm

namespace arancini::ir {
class chunk;
class node;
class port;
class packet;
} // namespace arancini::ir

namespace arancini::output::llvm {
class generation_context {
public:
	generation_context(const std::vector<std::shared_ptr<ir::chunk>> &chunks);

	void generate();

private:
	const std::vector<std::shared_ptr<ir::chunk>> &chunks_;
	std::unique_ptr<::llvm::LLVMContext> llvm_context_;
	std::unique_ptr<::llvm::Module> module_;

	struct {
		::llvm::Type *vd;
		::llvm::IntegerType *i1;
		::llvm::IntegerType *i8;
		::llvm::IntegerType *i16;
		::llvm::IntegerType *i32;
		::llvm::IntegerType *i64;
		::llvm::IntegerType *i128;
		::llvm::StructType *cpu_state;
		::llvm::PointerType *cpu_state_ptr;
		::llvm::FunctionType *main_fn;
		::llvm::FunctionType *loop_fn;
		::llvm::FunctionType *init_dbt;
		::llvm::FunctionType *dbt_invoke;
	} types;

	::llvm::MDNode *guest_mem_alias_scope_;

	void build();
	void initialise_types();
	void create_main_function(::llvm::Function *loop_fn);
	void optimise();
	void compile();
	void lower_chunks(::llvm::SwitchInst *pcswitch, ::llvm::BasicBlock *contblock);
	void lower_chunk(::llvm::SwitchInst *pcswitch, ::llvm::BasicBlock *contblock, std::shared_ptr<ir::chunk> chunk);
	::llvm::Value *lower_node(::llvm::IRBuilder<::llvm::ConstantFolder, ::llvm::IRBuilderDefaultInserter> &builder, ::llvm::Argument *start_arg,
		std::shared_ptr<ir::packet> pkt, ir::node *a);
	::llvm::Value *lower_port(::llvm::IRBuilder<::llvm::ConstantFolder, ::llvm::IRBuilderDefaultInserter> &builder, ::llvm::Argument *start_arg,
		std::shared_ptr<ir::packet> pkt, ir::port &p);
	::llvm::Value *materialise_port(::llvm::IRBuilder<::llvm::ConstantFolder, ::llvm::IRBuilderDefaultInserter> &builder, ::llvm::Argument *start_arg,
		std::shared_ptr<ir::packet> pkt, ir::port &p);
};

class llvm_output_engine : public output_engine {
public:
	void generate() override;
};
} // namespace arancini::output::llvm
