#include <arancini/ir/chunk.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/output/llvm/llvm-output-engine.h>
#include <iostream>
#include <map>
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
	types.i8 = Type::getInt8Ty(*llvm_context_);
	types.i32 = Type::getInt32Ty(*llvm_context_);
	types.i64 = Type::getInt64Ty(*llvm_context_);

	// CPU State

	// 0 PC, RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15,
	// 17 ZF, 18 CF, 19 OF, 20 SF
	auto state_elements = std::vector<Type *>({ types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, types.i64,
		types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, types.i8, types.i8, types.i8, types.i8 });

	types.cpu_state = StructType::get(*llvm_context_, state_elements, false);
	types.cpu_state->setName("cpu_state_struct");
	types.cpu_state_ptr = PointerType::get(types.cpu_state, 0);

	// Functions
	types.main_fn = FunctionType::get(types.i32, { types.i32, PointerType::get(Type::getInt8PtrTy(*llvm_context_), 0) }, false);
	types.loop_fn = FunctionType::get(types.vd, { types.cpu_state_ptr }, false);
	types.init_dbt = FunctionType::get(types.cpu_state_ptr, { types.i64 }, false);
	types.dbt_invoke = FunctionType::get(types.i32, { types.cpu_state_ptr }, false);
}

void generation_context::create_main_function(Function *loop_fn)
{
	auto main_fn = Function::Create(types.main_fn, GlobalValue::LinkageTypes::ExternalLinkage, "main", *module_);
	auto main_entry_block = BasicBlock::Create(*llvm_context_, "main_entry", main_fn);
	auto run_block = BasicBlock::Create(*llvm_context_, "run", main_fn);
	auto fail_block = BasicBlock::Create(*llvm_context_, "fail", main_fn);

	IRBuilder<> builder(*llvm_context_);
	builder.SetInsertPoint(main_entry_block);
	auto init_dbt_result
		= builder.CreateCall(module_->getOrInsertFunction("initialise_dynamic_runtime", types.init_dbt), { ConstantInt::get(types.i64, 0x4016b0) });

	auto is_not_null = builder.CreateCmp(CmpInst::Predicate::ICMP_NE, builder.CreatePtrToInt(init_dbt_result, types.i64), ConstantInt::get(types.i64, 0));

	builder.CreateCondBr(is_not_null, run_block, fail_block);

	builder.SetInsertPoint(run_block);
	builder.CreateCall(loop_fn, { init_dbt_result });
	builder.CreateRet(ConstantInt::get(types.i32, 0));

	builder.SetInsertPoint(fail_block);
	builder.CreateRet(ConstantInt::get(types.i32, 1));
}

void generation_context::build()
{
	initialise_types();

	auto loop_fn = Function::Create(types.loop_fn, GlobalValue::LinkageTypes::ExternalLinkage, "MainLoop", *module_);
	create_main_function(loop_fn);

	// TODO: Input Arch Specific (maybe need some kind of descriptor?)

	auto state_arg = loop_fn->getArg(0);

	auto entry_block = BasicBlock::Create(*llvm_context_, "entry", loop_fn);
	auto loop_block = BasicBlock::Create(*llvm_context_, "loop", loop_fn);
	auto switch_to_dbt = BasicBlock::Create(*llvm_context_, "switch_to_dbt", loop_fn);
	auto exit_block = BasicBlock::Create(*llvm_context_, "exit", loop_fn);

	IRBuilder<> builder(*llvm_context_);

	builder.SetInsertPoint(entry_block);

	// TODO: Input Arch Specific
	auto program_counter = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, 0) }, "pcptr");
	builder.CreateBr(loop_block);

	builder.SetInsertPoint(loop_block);
	auto program_counter_val = builder.CreateLoad(types.i64, program_counter, "pc");
	auto pcswitch = builder.CreateSwitch(program_counter_val, switch_to_dbt);

	lower_chunks(pcswitch, loop_block);

	builder.SetInsertPoint(switch_to_dbt);

	auto switch_callee = module_->getOrInsertFunction("invoke_code", types.dbt_invoke);
	auto invoke_result = builder.CreateCall(switch_callee, { state_arg });
	builder.CreateCondBr(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, invoke_result, ConstantInt::get(types.i32, 0)), loop_block, exit_block);

	builder.SetInsertPoint(exit_block);
	builder.CreateRetVoid();

	// module_->print(outs(), nullptr);

	if (verifyFunction(*loop_fn, &errs())) {
		throw std::runtime_error("function verification failed");
	}
}

void generation_context::lower_chunks(SwitchInst *pcswitch, BasicBlock *contblock)
{
	for (auto c : chunks_) {
		lower_chunk(pcswitch, contblock, c);
	}
}

static std::map<port *, Value *> node_ports_to_llvm_values;

Value *generation_context::lower_port(IRBuilder<> &builder, Argument *state_arg, std::shared_ptr<packet> pkt, port &p)
{
	auto existing = node_ports_to_llvm_values.find(&p);

	if (existing != node_ports_to_llvm_values.end()) {
		return existing->second;
	}

	auto n = p.owner();

	switch (p.owner()->kind()) {
	case node_kinds::constant: {
		auto cn = (constant_node *)n;

		switch (cn->val().type().width()) {
		case 1:
		case 8:
			return ConstantInt::get(types.i8, cn->const_val());
		case 32:
			return ConstantInt::get(types.i32, cn->const_val());
		case 64:
			return ConstantInt::get(types.i64, cn->const_val());

		default:
			throw std::runtime_error("unsupported constant width");
		}
	}

	case node_kinds::read_mem: {
		auto rmn = (read_mem_node *)n;
		auto address = lower_port(builder, state_arg, pkt, rmn->address());

		switch (rmn->val().type().width()) {
		case 8:
			return builder.CreateLoad(types.i8, builder.CreateIntToPtr(address, PointerType::get(types.i64, 256)));
		case 32:
			return builder.CreateLoad(types.i32, builder.CreateIntToPtr(address, PointerType::get(types.i64, 256)));
		case 64:
			return builder.CreateLoad(types.i64, builder.CreateIntToPtr(address, PointerType::get(types.i64, 256)));

		default:
			throw std::runtime_error("unsupported memory load width");
		}
		break;
	}

	case node_kinds::read_reg: {
		auto rrn = (read_reg_node *)n;
		auto src_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, rrn->regoff()) }, "regptr");

		switch (rrn->val().type().width()) {
		case 8:
			return builder.CreateLoad(types.i8, src_reg);
		case 32:
			return builder.CreateLoad(types.i32, src_reg);
		case 64:
			return builder.CreateLoad(types.i64, src_reg);

		default:
			throw std::runtime_error("unsupported register width " + std::to_string(rrn->val().type().width()) + " in load");
		}
	}

	case node_kinds::binary_arith: {
		auto ban = (binary_arith_node *)n;

		if (p.kind() == port_kinds::value) {
			auto lhs = lower_port(builder, state_arg, pkt, ban->lhs());
			auto rhs = lower_port(builder, state_arg, pkt, ban->rhs());

			switch (ban->op()) {
			case binary_arith_op::bxor:
				return builder.CreateXor(lhs, rhs);
			case binary_arith_op::band:
				return builder.CreateAnd(lhs, rhs);
			case binary_arith_op::add:
				return builder.CreateAdd(lhs, rhs);
			case binary_arith_op::sub:
				return builder.CreateSub(lhs, rhs);

			default:
				throw std::runtime_error("unsupported binary operator " + std::to_string((int)ban->op()));
			}
		} else {
			return ConstantInt::get(types.i8, 0);
			// throw std::runtime_error("unsupported port kind");
		}
	}

	case node_kinds::read_pc: {
		// auto src_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, 0) }, "pcptr");
		// return builder.CreateLoad(types.i64, src_reg);

		return ConstantInt::get(types.i64, pkt->get_start_node()->offset());
	}

	case node_kinds::cast: {
		auto cn = (cast_node *)n;

		auto val = lower_port(builder, state_arg, pkt, cn->source_value());

		switch (cn->op()) {
		case cast_op::zx:
			switch (cn->val().type().width()) {
			case 32:
				return builder.CreateZExt(val, types.i32);
			case 64:
				return builder.CreateZExt(val, types.i64);

			default:
				throw std::runtime_error("unsupported zx width");
			}

		default:
			throw std::runtime_error("unsupported cast op");
		}
	}

	default:
		throw std::runtime_error("unsupported port node kind " + std::to_string((int)n->kind()));
	}
}

Value *generation_context::lower_node(IRBuilder<> &builder, Argument *state_arg, std::shared_ptr<packet> pkt, node *a)
{
	switch (a->kind()) {
	case node_kinds::start:
	case node_kinds::end:
		return nullptr;

	case node_kinds::write_reg: {
		auto wrn = (write_reg_node *)a;
		// gep register
		// store value
		auto dest_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, wrn->regoff()) }, "regptr");

		auto val = lower_port(builder, state_arg, pkt, wrn->value());

		return builder.CreateStore(val, dest_reg);
	}

	case node_kinds::write_mem: {
		auto wmn = (write_mem_node *)a;

		auto address = lower_port(builder, state_arg, pkt, wmn->address());
		auto value = lower_port(builder, state_arg, pkt, wmn->value());

		return builder.CreateStore(value, builder.CreateIntToPtr(address, PointerType::get(types.i64, 256)));
	}

	case node_kinds::write_pc: {
		auto wpn = (write_pc_node *)a;

		auto dest_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, 0) }, "pcptr");

		auto val = lower_port(builder, state_arg, pkt, wpn->value());

		return builder.CreateStore(val, dest_reg);
	}

	default:
		throw std::runtime_error("unsupported node kind " + std::to_string((int)a->kind()));
	}
}

void generation_context::lower_chunk(SwitchInst *pcswitch, BasicBlock *contblock, std::shared_ptr<chunk> c)
{
	IRBuilder<> builder(*llvm_context_);
	std::map<unsigned long, BasicBlock *> blocks;

	if (c->packets().empty()) {
		return;
	}

	auto first_packet = c->packets().front();
	if (first_packet->actions().empty()) {
		return;
	}

	auto first_action = (start_node *)first_packet->actions().front();
	if (first_action->kind() != node_kinds::start) {
		throw std::logic_error("first action in first packet is not a start node");
	}

	BasicBlock *packet_block = BasicBlock::Create(*llvm_context_, "BB" + std::to_string(first_action->offset()), contblock->getParent());
	blocks[first_action->offset()] = packet_block;

	builder.SetInsertPoint(packet_block);

	auto state_arg = contblock->getParent()->getArg(0);

	for (auto p : c->packets()) {
		for (auto a : p->actions()) {
			lower_node(builder, state_arg, p, a);
		}
	}

	builder.CreateBr(contblock);

	for (auto b : blocks) {
		pcswitch->addCase(ConstantInt::get(types.i64, b.first), b.second);
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
