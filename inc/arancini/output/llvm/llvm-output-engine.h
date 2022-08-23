#pragma once

#include <arancini/output/output-engine.h>

namespace llvm {
class LLVMContext;
class Module;
} // namespace llvm

namespace arancini::output::llvm {
class llvm_output_engine : public output_engine {
public:
	void generate() override;

private:
	void build(::llvm::LLVMContext &ctx, ::llvm::Module &mod);
};
} // namespace arancini::output::llvm
