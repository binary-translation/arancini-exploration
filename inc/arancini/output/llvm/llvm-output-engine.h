#pragma once

#include <arancini/output/output-engine.h>
#include <memory>

namespace llvm {
class LLVMContext;
class Module;
class FunctionType;
class Type;
class PointerType;
class StructType;
} // namespace llvm

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
		::llvm::Type *i32;
		::llvm::Type *i64;
		::llvm::StructType *cpu_state;
		::llvm::PointerType *cpu_state_ptr;
		::llvm::FunctionType *main_fn;
		::llvm::FunctionType *loop_fn;
		::llvm::FunctionType *init_dbt;
		::llvm::FunctionType *dbt_invoke;
	} types;

	void build();
	void initialise_types();
	void create_main_function();
	void optimise();
	void compile();
};

class llvm_output_engine : public output_engine {
public:
	void generate() override;
};
} // namespace arancini::output::llvm
