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
	generation_context gc(chunks());
	gc.generate();
}

generation_context::generation_context(const std::vector<std::shared_ptr<ir::chunk>> &chunks)
	: chunks_(chunks)
	, llvm_context_(std::make_unique<LLVMContext>())
	, module_(std::make_unique<Module>("test", *llvm_context_))
{
}

void generation_context::generate()
{
	InitializeAllTargetInfos();
	InitializeAllTargets();
	InitializeAllTargetMCs();
	InitializeAllAsmParsers();
	InitializeAllAsmPrinters();

	build();
	optimise();
	compile();
}

void generation_context::initialise_types()
{
	// Primitives
	types.vd = Type::getVoidTy(*llvm_context_);
	types.i32 = Type::getInt32Ty(*llvm_context_);
	types.i64 = Type::getInt64Ty(*llvm_context_);

	// CPU State
	auto state_elements = std::vector<Type *>({ types.i64 });
	types.cpu_state = StructType::get(*llvm_context_, state_elements, false);
	types.cpu_state->setName("cpu_state_struct");
	types.cpu_state_ptr = PointerType::get(types.cpu_state, 0);

	// Functions
	types.main_fn = FunctionType::get(types.i32, { types.i32, PointerType::get(Type::getInt8PtrTy(*llvm_context_), 0) }, false);
	types.loop_fn = FunctionType::get(types.vd, { types.cpu_state_ptr }, false);
	types.init_dbt = FunctionType::get(types.vd, false);
	types.dbt_invoke = FunctionType::get(types.vd, { types.cpu_state_ptr }, false);
}

void generation_context::create_main_function()
{
	auto main_fn = Function::Create(types.main_fn, GlobalValue::LinkageTypes::ExternalLinkage, "main", *module_);
	auto main_entry_block = BasicBlock::Create(*llvm_context_, "main_entry", main_fn);

	IRBuilder<> builder(*llvm_context_);
	builder.SetInsertPoint(main_entry_block);
	builder.CreateCall(module_->getOrInsertFunction("initialise_dynamic_runtime", types.init_dbt));

	// TODO: Initialise this - PC needs to be ELF Entry Point
	auto global_cpu_state = module_->getOrInsertGlobal("GlobalCPUState", types.cpu_state);

	builder.CreateCall(module_->getOrInsertFunction("MainLoop", types.loop_fn), { global_cpu_state });

	builder.CreateRet(ConstantInt::get(types.i32, 0));
}

void generation_context::build()
{
	initialise_types();
	create_main_function();

	// TODO: Input Arch Specific (maybe need some kind of descriptor?)

	auto loop_fn = Function::Create(types.loop_fn, GlobalValue::LinkageTypes::ExternalLinkage, "MainLoop", *module_);
	auto state_arg = loop_fn->getArg(0);

	auto entry_block = BasicBlock::Create(*llvm_context_, "entry", loop_fn);
	auto loop_block = BasicBlock::Create(*llvm_context_, "loop", loop_fn);
	auto switch_to_dbt = BasicBlock::Create(*llvm_context_, "switch_to_dbt", loop_fn);

	IRBuilder<> builder(*llvm_context_);

	builder.SetInsertPoint(entry_block);

	// TODO: Input Arch Specific
	auto program_counter = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, 0) }, "pcptr");
	builder.CreateBr(loop_block);

	builder.SetInsertPoint(loop_block);
	auto program_counter_val = builder.CreateLoad(types.i64, program_counter, "pc");
	builder.CreateSwitch(program_counter_val, switch_to_dbt);

	builder.SetInsertPoint(switch_to_dbt);

	auto switch_callee = module_->getOrInsertFunction("invoke_code", types.dbt_invoke);
	builder.CreateCall(switch_callee, { state_arg });
	builder.CreateBr(loop_block);

	if (verifyFunction(*loop_fn, &errs())) {
		throw std::runtime_error("function verification failed");
	}
}

void generation_context::optimise()
{
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

	MPM.run(*module_, MAM);

	// Compiled modules now exists
	module_->print(outs(), nullptr);
}

void generation_context::compile()
{
	auto TT = sys::getDefaultTargetTriple();
	module_->setTargetTriple(TT);

	std::string error_message;
	auto T = TargetRegistry::lookupTarget(TT, error_message);
	if (!T) {
		throw std::runtime_error(error_message);
	}

	TargetOptions TO;
	auto RM = Optional<Reloc::Model>();
	auto TM = T->createTargetMachine(TT, "generic", "", TO, RM);

	module_->setDataLayout(TM->createDataLayout());

	std::error_code EC;
	raw_fd_ostream output_file("generated.o", EC, sys::fs::OF_None);

	if (EC) {
		throw std::runtime_error("could not create output file: " + EC.message());
	}

	legacy::PassManager OPM;
	if (TM->addPassesToEmitFile(OPM, output_file, nullptr, CGFT_ObjectFile)) {
		throw std::runtime_error("unable to emit file");
	}

	OPM.run(*module_);
}
