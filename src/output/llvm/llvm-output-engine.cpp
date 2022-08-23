#include <arancini/ir/chunk.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/output/llvm/llvm-output-engine.h>
#include <iostream>
#include <string>

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
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

using namespace arancini::output::llvm;
using namespace arancini::ir;
using namespace ::llvm;

void llvm_output_engine::generate()
{
	InitializeAllTargetInfos();
	InitializeAllTargets();
	InitializeAllTargetMCs();
	InitializeAllAsmParsers();
	InitializeAllAsmPrinters();

	LLVMContext ctx;
	Module mod("test", ctx);

	build(ctx, mod);

	LoopAnalysisManager LAM;
	FunctionAnalysisManager FAM;
	CGSCCAnalysisManager CGAM;
	ModuleAnalysisManager MAM;
	PassBuilder PB;

	PB.registerModuleAnalyses(MAM);
	PB.registerCGSCCAnalyses(CGAM);
	PB.registerFunctionAnalyses(FAM);
	PB.registerLoopAnalyses(LAM);
	PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

	ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O2);

	MPM.run(mod, MAM);

	auto TT = sys::getDefaultTargetTriple();
	mod.setTargetTriple(TT);

	// Compiled modules now exists
	mod.print(outs(), nullptr);

	std::string error_message;
	auto T = TargetRegistry::lookupTarget(TT, error_message);
	if (!T) {
		throw std::runtime_error(error_message);
	}

	TargetOptions TO;
	auto RM = Optional<Reloc::Model>();
	auto TM = T->createTargetMachine(TT, "generic", "", TO, RM);

	mod.setDataLayout(TM->createDataLayout());

	std::error_code EC;
	raw_fd_ostream output_file("generated.o", EC, sys::fs::OF_None);

	if (EC) {
		throw std::runtime_error("could not create output file: " + EC.message());
	}

	legacy::PassManager OPM;
	if (TM->addPassesToEmitFile(OPM, output_file, nullptr, CGFT_ObjectFile)) {
		throw std::runtime_error("unable to emit file");
	}

	OPM.run(mod);
}

void llvm_output_engine::build(LLVMContext &ctx, Module &mod)
{
	auto main_fn_type = FunctionType::get(Type::getInt32Ty(ctx), { Type::getInt32Ty(ctx), PointerType::get(Type::getInt8PtrTy(ctx), 0) }, false);
	auto main_fn = Function::Create(main_fn_type, GlobalValue::LinkageTypes::ExternalLinkage, "main", mod);

	auto state_elements = std::vector<Type *>({ Type::getInt64Ty(ctx) });
	auto cpu_state_type = StructType::get(ctx, state_elements, false);
	cpu_state_type->setName("cpu_state_struct");
	auto cpu_state_ptr_type = PointerType::get(cpu_state_type, 0);

	auto loop_fn_type = FunctionType::get(Type::getVoidTy(ctx), { cpu_state_ptr_type }, false);

	auto loop_fn = Function::Create(loop_fn_type, GlobalValue::LinkageTypes::ExternalLinkage, "MainLoop", mod);
	auto state_arg = loop_fn->getArg(0);

	auto main_entry_block = BasicBlock::Create(ctx, "main_entry", main_fn);
	auto entry_block = BasicBlock::Create(ctx, "entry", loop_fn);
	auto loop_block = BasicBlock::Create(ctx, "loop", loop_fn);
	auto switch_to_dbt = BasicBlock::Create(ctx, "switch_to_dbt", loop_fn);

	auto init_dbt_type = FunctionType::get(Type::getVoidTy(ctx), false);

	IRBuilder<> builder(ctx);
	builder.SetInsertPoint(main_entry_block);
	builder.CreateCall(mod.getOrInsertFunction("initialise_dynamic_runtime", init_dbt_type));

	auto global_cpu_state = mod.getOrInsertGlobal("GlobalCPUState", cpu_state_type);

	builder.CreateCall(mod.getOrInsertFunction("MainLoop", loop_fn_type), { global_cpu_state });

	builder.CreateRet(ConstantInt::get(Type::getInt32Ty(ctx), 0));

	builder.SetInsertPoint(entry_block);
	auto program_counter
		= builder.CreateGEP(cpu_state_type, state_arg, { ConstantInt::get(Type::getInt64Ty(ctx), 0), ConstantInt::get(Type::getInt32Ty(ctx), 0) }, "pcptr");
	builder.CreateBr(loop_block);

	builder.SetInsertPoint(loop_block);
	auto program_counter_val = builder.CreateLoad(Type::getInt64Ty(ctx), program_counter, "pc");
	builder.CreateSwitch(program_counter_val, switch_to_dbt);

	builder.SetInsertPoint(switch_to_dbt);

	auto switch_callee = mod.getOrInsertFunction("invoke_code", loop_fn_type);
	builder.CreateCall(switch_callee, { state_arg });
	builder.CreateBr(loop_block);

	if (verifyFunction(*loop_fn, &errs())) {
		throw std::runtime_error("function verification failed");
	}

	/*dot_graph_generator g(std::cout);

	for (auto c : chunks()) {
		c->accept(g);
	}*/
}
