#include "arancini/ir/node.h"
#include "arancini/ir/opt.h"
#include "arancini/ir/port.h"
#include "arancini/ir/visitor.h"
#include "arancini/output/static/llvm/llvm-static-visitor.h"
#include "arancini/output/static/llvm/llvm-fence-combine.h"
#include "arancini/runtime/exec/x86/x86-cpu-state.h"
#include "arancini/util/logger.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Config/llvm-config.h"
#include <arancini/ir/chunk.h>
#include <arancini/output/static/llvm/llvm-static-output-engine-impl.h>
#include <arancini/output/static/llvm/llvm-static-output-engine.h>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/FloatingPointMode.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/ConstantFolder.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include "llvm/IR/GlobalAlias.h"
#include <llvm/Support/CodeGen.h>
#include <llvm/Transforms/Utils/Mem2Reg.h>
#include "llvm/Transforms/AggressiveInstCombine/AggressiveInstCombine.h"
#include "llvm/Transforms/IPO/DeadArgumentElimination.h"
#include "llvm/Transforms/Scalar/ADCE.h"
#include <llvm/IR/PassManager.h>
#include <llvm/Support/TimeProfiler.h>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#if LLVM_VERSION_MAJOR < 16
#include <llvm/ADT/Optional.h>
template <typename T>
using optional = llvm::Optional<T>;
#else
#include <optional>
template <typename T>
using optional = std::optional<T>;
#endif

using namespace arancini::output::o_static::llvm;
using namespace arancini::ir;
using namespace ::llvm;
using arancini::input::x86::off_to_idx;
using arancini::input::x86::regnames;

llvm_static_output_engine::llvm_static_output_engine(const std::string &output_filename, const bool is_exec)
	: static_output_engine(output_filename)
	, oei_(std::make_unique<llvm_static_output_engine_impl>(*this, extern_fns(), chunks()))
	, dbg_(false)
	, is_exec_(is_exec)
{
}

llvm_static_output_engine::~llvm_static_output_engine() = default;

void llvm_static_output_engine::generate() { oei_->generate(); }

llvm_static_output_engine_impl::llvm_static_output_engine_impl(const llvm_static_output_engine &e, const std::vector<std::pair<unsigned long, std::string>> &extern_fns, const std::vector<std::shared_ptr<ir::chunk>> &chunks)
	: e_(e)
	, extern_fns_(extern_fns)
	, chunks_(chunks)
	, llvm_context_(std::make_unique<LLVMContext>())
	, module_(std::make_unique<Module>("generated", *llvm_context_))
	, in_br(false)
	, fixed_branches(0)
{
}

void llvm_static_output_engine_impl::generate()
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



void llvm_static_output_engine_impl::initialise_types()
{
	// Primitives
	types.vd = Type::getVoidTy(*llvm_context_);
	types.i1 = Type::getInt1Ty(*llvm_context_);
	types.i8 = Type::getInt8Ty(*llvm_context_);
	types.i16 = Type::getInt16Ty(*llvm_context_);
	types.i32 = Type::getInt32Ty(*llvm_context_);
	types.i64 = Type::getInt64Ty(*llvm_context_);
	types.f32 = Type::getFloatTy(*llvm_context_);
	types.f64 = Type::getDoubleTy(*llvm_context_);
	types.f80 = Type::getDoubleTy(*llvm_context_);
	types.i128 = Type::getInt128Ty(*llvm_context_);
	types.i256 = IntegerType::get(*llvm_context_, 256);
	types.i512 = IntegerType::get(*llvm_context_, 512);

	// CPU State

	// 0 PC, RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15,
	// 17 ZF, 18 CF, 19 OF, 20 SF, 21 PF
	// 22 XMM0...
	// 38 FS, 38 GS
	auto state_elements = std::vector<Type *>({
	/*types.i64, // 0: RIP
	types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, // 1: AX, CX, DX, BX, SP, BP, SI, DI
	types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, // 9: R8, R9, R10, R11, R12, R13, R14, R15
	types.i8, types.i8, types.i8, types.i8, types.i8, // 17: ZF, CF, OF, SF, PF
	types.i128, types.i128, types.i128, types.i128, types.i128, types.i128, types.i128, types.i128, // 22: XMM0--7
	types.i128, types.i128, types.i128, types.i128, types.i128, types.i128, types.i128, types.i128, // 30: XMM8--15
	types.i64, types.i64 // 38: FS, GS*/

#define DEFREG(ctype, ltype, name) types.ltype,
#include <arancini/input/x86/reg.def>
#undef DEFREG
	});

	types.cpu_state = StructType::get(*llvm_context_, state_elements,true);
	if (!types.cpu_state->isLiteral())
		types.cpu_state->setName("cpu_state_struct");
	types.cpu_state_ptr = PointerType::get(types.cpu_state, 0);

	// Functions
	types.main_fn = FunctionType::get(types.i32, { types.i32, PointerType::get(PointerType::get(Type::getInt8Ty(*llvm_context_), 0), 0)}, false);
	types.loop_fn = FunctionType::get(types.vd, { types.cpu_state_ptr }, false);
	types.chunk_fn = FunctionType::get(VectorType::get(types.i64, 3, false), { types.cpu_state_ptr, types.i64, types.i64, types.i64, types.i64, types.i64 }, false);	// (state, pc, eax, ecx, edx, rsp) -> { pc, eax, rsp } // fastcall|thiscall|cdecl
	types.init_dbt = FunctionType::get(types.cpu_state_ptr, { types.i64, types.i32, PointerType::get(PointerType::get(Type::getInt8Ty(*llvm_context_), 0),0) }, false);
	types.dbt_invoke = FunctionType::get(types.i32, { types.cpu_state_ptr }, false);
	types.internal_call_handler = FunctionType::get(types.i32, { types.cpu_state_ptr, types.i32 }, false);
	types.finalize = FunctionType::get(types.vd, {}, false);
	types.clk_fn = FunctionType::get(types.vd, { types.cpu_state_ptr, PointerType::get(PointerType::get(Type::getInt8Ty(*llvm_context_), 0), 0) }, false);
	types.register_static_fn = FunctionType::get(Type::getVoidTy(*llvm_context_), {types.i64, types.i8->getPointerTo()}, false);
	types.lookup_static_fn = FunctionType::get(types.i8->getPointerTo(), { types.i64 }, false);
	types.poison_fn = FunctionType::get(Type::getVoidTy(*llvm_context_), PointerType::getInt8Ty(*llvm_context_));
}

void llvm_static_output_engine_impl::create_main_function(Function *loop_fn)
{
	auto main_fn = Function::Create(types.main_fn, GlobalValue::LinkageTypes::ExternalLinkage, "main", *module_);
	auto main_entry_block = BasicBlock::Create(*llvm_context_, "main_entry", main_fn);
	auto run_block = BasicBlock::Create(*llvm_context_, "run", main_fn);
	auto fail_block = BasicBlock::Create(*llvm_context_, "fail", main_fn);

	IRBuilder<> builder(*llvm_context_);
	builder.SetInsertPoint(main_entry_block);
	auto init_dbt_result = builder.CreateCall(module_->getOrInsertFunction("initialise_dynamic_runtime", types.init_dbt),
		{ ConstantInt::get(types.i64, e_.get_entrypoint()), main_fn->getArg(0), main_fn->getArg(1) });

	auto is_not_null = builder.CreateCmp(CmpInst::Predicate::ICMP_NE, builder.CreatePtrToInt(init_dbt_result, types.i64), ConstantInt::get(types.i64, 0));

	builder.CreateCondBr(is_not_null, run_block, fail_block);

	builder.SetInsertPoint(run_block);

	auto registerFn = module_->getOrInsertFunction("register_static_fn_addr", types.register_static_fn);
	for (auto it = fns_->begin(); it != fns_->end(); ++it) {
		builder.CreateCall(registerFn, {builder.getInt64(it->first), it->second});
	}

	builder.CreateCall(loop_fn, { init_dbt_result });
	builder.CreateRet(ConstantInt::get(types.i32, 0));

	builder.SetInsertPoint(fail_block);
	builder.CreateRet(ConstantInt::get(types.i32, 1));
}

void llvm_static_output_engine_impl::create_function_decls() {

	for (auto n : extern_fns_) {
		auto fn = static_cast<Function *>(module_->getOrInsertFunction(n.second, get_fn_type()).getCallee());
		(*fns_)[n.first] = fn;
	}
}

void llvm_static_output_engine_impl::create_static_functions()
{
	for (auto c : chunks_) {

		off_t addr = c->packets()[0]->address();
		if (addr != 0) {

			auto it = fns_->find(addr);
			if (it != fns_->end()) {

				auto old = it->second;
				GlobalAlias::create(get_fn_type(), old->getAddressSpace(), GlobalValue::LinkageTypes::ExternalLinkage, c->name(), old, module_.get());
				continue;
			}
			auto fn = Function::Create(get_fn_type(), GlobalValue::LinkageTypes::ExternalLinkage, c->name(), *module_);
			fn->addParamAttr(0, Attribute::AttrKind::NonNull);
			fn->addParamAttr(0, Attribute::AttrKind::NoAlias);
			fn->addParamAttr(0, Attribute::AttrKind::NoCapture);
			fn->addParamAttr(0, Attribute::getWithDereferenceableBytes(*llvm_context_, sizeof(runtime::exec::x86::x86_cpu_state)));
			fn->addParamAttr(0, Attribute::AttrKind::NoUndef);
			fn->addParamAttr(0, Attribute::getWithAlignment(*llvm_context_, Align(16)));
			(*fns_)[addr] = fn;
		} else {
			// Wrapper
			auto fn = Function::Create(get_fn_type(), GlobalValue::LinkageTypes::ExternalLinkage, c->name(), *module_);
			fn->addParamAttr(0, Attribute::AttrKind::NonNull);
			fn->addParamAttr(0, Attribute::AttrKind::NoAlias);
			fn->addParamAttr(0, Attribute::AttrKind::NoCapture);
			fn->addParamAttr(0, Attribute::getWithDereferenceableBytes(*llvm_context_, sizeof(runtime::exec::x86::x86_cpu_state)));
			fn->addParamAttr(0, Attribute::AttrKind::NoUndef);
			fn->addParamAttr(0, Attribute::getWithAlignment(*llvm_context_, Align(16)));
			(*wrapper_fns_)[c->name()] = fn;
		}
	}
}

void llvm_static_output_engine_impl::build()
{
	initialise_types();

	//! 0 = !{!1}
	//! 1 = distinct !{!1, !2, !"MainLoop: argument 0"}
	//! 2 = distinct !{!2, !"MainLoop"}

	MDBuilder mdb(*llvm_context_);
	auto fmdom = mdb.createAnonymousAliasScopeDomain("guest-mem");
	guest_mem_alias_scope_ = mdb.createAnonymousAliasScope(fmdom, "guest-mem-scope");

	auto rfdom = mdb.createAnonymousAliasScopeDomain("reg-file");
	reg_file_alias_scope_ = mdb.createAnonymousAliasScope(rfdom, "reg-file-scope");

	create_static_functions();
	create_function_decls();

	// Only the executable needs a MainLoop and main
	Function *loop_fn;
	if (e_.is_exec()) {
		loop_fn = Function::Create(types.loop_fn, GlobalValue::LinkageTypes::ExternalLinkage, "MainLoop", *module_);
		loop_fn->addParamAttr(0, Attribute::AttrKind::NoCapture);
		loop_fn->addParamAttr(0, Attribute::AttrKind::NoAlias);
		loop_fn->addParamAttr(0, Attribute::AttrKind::NoUndef);
		loop_fn->addParamAttr(0, Attribute::getWithAlignment(*llvm_context_, Align(16)));

		create_main_function(loop_fn);
	} else {
		loop_fn = static_cast<Function *>(module_->getOrInsertFunction("MainLoop", types.loop_fn).getCallee());
	}

	// TODO: Input Arch Specific (maybe need some kind of descriptor?)

	if (!e_.is_exec()) {
		lower_chunks(loop_fn);

		{
			func_map_.push_back(ConstantPointerNull::get(PointerType::get(*llvm_context_, 0)));
			func_map_.push_back(ConstantPointerNull::get(PointerType::get(*llvm_context_, 0)));

			ArrayType *ty = ArrayType::get(PointerType::get(*llvm_context_, 0), func_map_.size());
			auto *gvar = reinterpret_cast<GlobalVariable *>(module_->getOrInsertGlobal("__FUNCMAP", ty));
			gvar->setInitializer(ConstantArray::get(ty, func_map_));
			gvar->setLinkage(GlobalValue::ExternalLinkage);
			gvar->setVisibility(GlobalValue::HiddenVisibility);
		}

		debug_dump();
		return;
	}
	auto state_arg = loop_fn->getArg(0);

	auto entry_block = BasicBlock::Create(*llvm_context_, "entry", loop_fn);
	auto loop_block = BasicBlock::Create(*llvm_context_, "loop", loop_fn);
	auto switch_to_dbt = BasicBlock::Create(*llvm_context_, "switch_to_dbt", loop_fn);
	auto check_for_call_block = BasicBlock::Create(*llvm_context_, "check_for_call", loop_fn);
	auto check_for_ret_block = BasicBlock::Create(*llvm_context_, "check_for_ret", loop_fn);
	auto check_for_int_call_block = BasicBlock::Create(*llvm_context_, "check_for_internal_call", loop_fn);
	auto internal_call_block = BasicBlock::Create(*llvm_context_, "internal_call", loop_fn);
	auto call_block = BasicBlock::Create(*llvm_context_, "call", loop_fn);
	auto ret_block = BasicBlock::Create(*llvm_context_, "return", loop_fn);
	auto exit_block = BasicBlock::Create(*llvm_context_, "exit", loop_fn);

	auto clk_ = module_->getOrInsertFunction("clk", types.clk_fn);
	IRBuilder<> builder(*llvm_context_);

	builder.SetInsertPoint(entry_block);

#if defined(DEBUG)
	builder.CreateCall(clk_, {state_arg, builder.CreateGlobalStringPtr("do-loop")});
#endif

	// TODO: Input Arch Specific
	auto program_counter = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, 0) }, "pcptr");
	builder.CreateBr(loop_block);

	builder.SetInsertPoint(loop_block);

	//DEBUG
	//auto alert = module_->getOrInsertFunction("alert", types.finalize);
	//builder.CreateCall(alert, { });
	auto program_counter_val = builder.CreateLoad(types.i64, program_counter, "top_pc");
	lower_static_fn_lookup(builder, switch_to_dbt, program_counter_val);

	lower_chunks(loop_fn);

	builder.SetInsertPoint(switch_to_dbt);

	auto switch_callee = module_->getOrInsertFunction("invoke_code", types.dbt_invoke);
#if defined(DEBUG)
	builder.CreateCall(clk_, {state_arg, builder.CreateGlobalStringPtr("do-dyn")});
#endif
	auto invoke_result = builder.CreateCall(switch_callee, { state_arg });
#if defined(DEBUG)
	builder.CreateCall(clk_, {state_arg, builder.CreateGlobalStringPtr("done-dyn")});
#endif
	/*
	 * RETURN CODES:
	 * 4: last instr was a ret  -> emit a return
	 * 3: last instr was a call -> call MainLoop to figure out the next Fn
	 * 2: do internal call
	 * 1: do syscall
	 * 0: all other instr		-> we did not leave the current unknown function
	 */
	builder.CreateCondBr(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, invoke_result, ConstantInt::get(types.i32, 0)), loop_block, check_for_call_block);

	builder.SetInsertPoint(check_for_call_block);
	builder.CreateCondBr(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, invoke_result, ConstantInt::get(types.i32, 3)), call_block, check_for_ret_block);

	builder.SetInsertPoint(check_for_ret_block);
	builder.CreateCondBr(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, invoke_result, ConstantInt::get(types.i32, 4)), ret_block, check_for_int_call_block);

	builder.SetInsertPoint(check_for_int_call_block);
	builder.CreateCondBr(builder.CreateCmp(CmpInst::Predicate::ICMP_SGT, invoke_result, ConstantInt::get(types.i32, 0)), internal_call_block, exit_block);

	builder.SetInsertPoint(internal_call_block);
#if defined(DEBUG)
	builder.CreateCall(clk_, {state_arg, builder.CreateGlobalStringPtr("do-internal")});
#endif
	auto internal_call_callee = module_->getOrInsertFunction("execute_internal_call", types.internal_call_handler);
#if defined(DEBUG)
	builder.CreateCall(clk_, {state_arg, builder.CreateGlobalStringPtr("done-internal")});
#endif
	auto internal_call_result = builder.CreateCall(internal_call_callee, { state_arg, invoke_result });
	builder.CreateCondBr(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, internal_call_result, ConstantInt::get(types.i32, 0)), loop_block, exit_block);


	builder.SetInsertPoint(exit_block);

	auto finalize_call_callee = module_->getOrInsertFunction("finalize", types.finalize);
	builder.CreateCall(finalize_call_callee, {});

	builder.CreateRetVoid();

	builder.SetInsertPoint(call_block);
	builder.CreateCall(loop_fn, { state_arg });
	builder.CreateBr(loop_block);

	builder.SetInsertPoint(ret_block);
#if defined(DEBUG)
	builder.CreateCall(clk_, {state_arg, builder.CreateGlobalStringPtr("done-loop")});
#endif
	builder.CreateRetVoid();

	debug_dump();

	if (verifyFunction(*loop_fn, &errs())) {
		throw std::runtime_error("function verification failed");
	}
}

void llvm_static_output_engine_impl::debug_dump() {
	if (e_.debug_dump_filename.has_value()) {
		std::error_code EC;
		std::string filename = e_.debug_dump_filename.value() + ".ll";
		raw_fd_ostream file(filename, EC);

		if (EC) {
			errs() << "Error opening file '" << filename << "': " << EC.message() << "\n";
		}

		module_->print(file, nullptr);
		file.close();
	}
}

Value *llvm_static_output_engine_impl::createLoadFromCPU(IRBuilder<> &builder, Argument *state_arg, unsigned long reg_idx) {

	Type *ty = types.i512;
	if ( reg_idx < 27 ) ty = types.i64;

	return builder.CreateLoad(ty, builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, reg_idx) }), "Load_Idx_"+itostr(reg_idx));
}

void llvm_static_output_engine_impl::createStoreToCPU(IRBuilder<> &builder, Argument *state_arg, unsigned int ret_idx, Value *ret, unsigned long reg_idx) {

		builder.CreateStore(builder.CreateExtractValue(ret, { ret_idx }), builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, reg_idx) }));
}

void llvm_static_output_engine_impl::lower_chunks(Function *main_loop)
{
	IRBuilder<> builder(*llvm_context_);

	auto ret = llvm_ret_visitor();
	auto arg = llvm_arg_visitor();

	for (const auto &c : chunks_) {
		c->accept(ret);
		c->accept(arg);
	}

	for (const auto &c : chunks_) {
		lower_chunk(&builder, main_loop, c);
	}
}

void llvm_static_output_engine_impl::lower_static_fn_lookup(IRBuilder<> &builder, BasicBlock *contblock, Value *guestAddr) {
	auto LookupFn = module_->getOrInsertFunction("lookup_static_fn_addr", types.lookup_static_fn);

	auto clk_ = module_->getOrInsertFunction("clk", types.clk_fn);

	auto result = builder.CreateCall(LookupFn, { guestAddr });
	auto cmp = builder.CreateCmp(CmpInst::Predicate::ICMP_NE, result, ConstantPointerNull::get(PointerType::get(types.i8, 0)));

	auto b = BasicBlock::Create(*llvm_context_, "call_static_fn", contblock->getParent());

	builder.CreateCondBr(cmp, b, contblock);
	auto cpu_state = contblock->getParent()->getArg(0);

	builder.SetInsertPoint(b);
	auto rdi = createLoadFromCPU(builder, cpu_state, 8);
	auto rsi = createLoadFromCPU(builder, cpu_state, 7);
	auto rdx = createLoadFromCPU(builder, cpu_state, 3);
	auto rcx = createLoadFromCPU(builder, cpu_state, 2);
	auto r8 = createLoadFromCPU(builder, cpu_state, 9);
	auto r9 = createLoadFromCPU(builder, cpu_state, 10);
	//	auto zmm0 = createLoadFromCPU(builder, cpu_state, 27);
	//	auto zmm1 = createLoadFromCPU(builder, cpu_state, 28);
	//	auto zmm2 = createLoadFromCPU(builder, cpu_state, 29);
	//	auto zmm3 = createLoadFromCPU(builder, cpu_state, 30);
	//	auto zmm4 = createLoadFromCPU(builder, cpu_state, 31);
	//	auto zmm5 = createLoadFromCPU(builder, cpu_state, 32);
	//	auto zmm6 = createLoadFromCPU(builder, cpu_state, 33);
	//	auto zmm7 = createLoadFromCPU(builder, cpu_state, 34);
	auto f = builder.CreateBitCast(result, get_fn_type()->getPointerTo());
	auto call = builder.CreateCall(get_fn_type(), f, { cpu_state, rdi, rsi, rdx, rcx, r8, r9 /*, zmm0, zmm1, zmm2, zmm3, zmm4, zmm5, zmm6, zmm7*/ });
	createStoreToCPU(builder, cpu_state, 0, call, 1);
	createStoreToCPU(builder, cpu_state, 1, call, 3);
//	createStoreToCPU(builder, cpu_state, 2, call, 27);
//	createStoreToCPU(builder, cpu_state, 3, call, 28);
#if defined(DEBUG)
	builder.CreateCall(clk_, {cpu_state, builder.CreateGlobalStringPtr("done-loop")});
#endif
	builder.CreateRetVoid();
}

Value *llvm_static_output_engine_impl::materialise_port(IRBuilder<> &builder, Argument *state_arg, std::shared_ptr<packet> pkt, port &p)
{
	auto n = p.owner();

	switch (p.owner()->kind()) {
	case node_kinds::constant: {
		auto cn = (constant_node *)n;

		::llvm::Type *ty;
		auto is_f = cn->val().type().is_floating_point();
		switch (cn->val().type().width()) {
		case 1:
		// TODO: check if that makes sense
		case 3:
		case 8:
			ty = types.i8;
			break;
		case 16:
			ty = types.i16;
			break;
		case 32: {
			ty = types.i32;
			if (is_f)
				ty = types.f32;
			break;
		}
		case 64: {
			ty = types.i64;
			if (is_f)
				ty = types.f64;
			break;
		}
		case 80:
			ty = types.f80;
			break;
		default:
			throw std::runtime_error("unsupported constant width: " + std::to_string(cn->val().type().width()));
		}
		if (is_f)
			return ConstantFP::get(ty, cn->const_val_f());
		return ConstantInt::get(ty, cn->const_val_i());
	}

	case node_kinds::read_mem: {
		auto rmn = (read_mem_node *)n;
		auto address = lower_port(builder, state_arg, pkt, rmn->address());

		::llvm::Type *ty;
		switch (rmn->val().type().width()) {
		case 8:
			ty = types.i8;
			break;
		case 16:
			ty = types.i16;
			break;
		case 32:
			ty = types.i32;
			break;
		case 64:
			ty = types.i64;
			break;
		case 80:
			ty = types.f80;
			break;
		case 128:
			ty = types.i128;
			break;

		default:
			throw std::runtime_error("unsupported memory load width: " + std::to_string(rmn->val().type().width()));
		}

#ifndef ARCH_X86_64
		//auto gs_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, 26) }); //TODO: move offset_2_idx into a common header
		auto gs_reg = reg_to_alloca_.at(reg_offsets::GS);
		//address = builder.CreateAdd(address, builder.CreateLoad(types.i64, gs_reg));
#endif
		auto address_ptr = builder.CreateIntToPtr(address, PointerType::get(ty, 256));

		LoadInst *li = builder.CreateLoad(ty, address_ptr);
		if (e_.fences_){
			if (!is_stack(rmn))
				builder.CreateFence(AtomicOrdering::Acquire);
		}
		li->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(li->getContext(), guest_mem_alias_scope_));
		li->setMetadata(LLVMContext::MD_noalias, MDNode::get(li->getContext(), reg_file_alias_scope_));
		return li;
	}

	case node_kinds::read_reg: {
		auto rrn = (read_reg_node *)n;
		//auto src_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, rrn->regidx()) },
		//	idx_to_reg_name(rrn->regidx()));
		auto src_reg = reg_to_alloca_.at((reg_offsets)rrn->regoff());

		::llvm::Type *ty;
		Align align = Align(8);
		switch (rrn->val().type().width()) {
		case 1:
			ty = types.i1;
			align = Align(1);
			break;
		case 8:
			ty = types.i8;
			align = Align(1);
			break;
		case 16:
			ty = types.i16;
			break;
		case 32:
			ty = types.i32;
			break;
		case 64:
			ty = types.i64;
			break;
		case 128:
			ty = types.i128;
			align = Align(64);
			break;
		case 256:
			ty = types.i256;
			align = Align(64);
			break;
		case 512:
			ty = types.i512;
			align = Align(64);
			break;
		default:
			throw std::runtime_error("unsupported register width " + std::to_string(rrn->val().type().width()) + " in load");
		}
		return builder.CreateAlignedLoad(ty, src_reg, align);
	}

	case node_kinds::binary_arith: {
		auto ban = (binary_arith_node *)n;
		auto lhs = lower_port(builder, state_arg, pkt, ban->lhs());
		auto rhs = lower_port(builder, state_arg, pkt, ban->rhs());

		if (p.kind() == port_kinds::value) {

			// Sometimes we need to extend some types
			// For example when comparing a flag to an immediate
			if (lhs->getType()->isIntegerTy()) {
				if (lhs->getType()->getIntegerBitWidth() > rhs->getType()->getPrimitiveSizeInBits())
					rhs = builder.CreateSExt(rhs, lhs->getType());

				if (lhs->getType()->getIntegerBitWidth() < rhs->getType()->getPrimitiveSizeInBits())
					lhs = builder.CreateSExt(lhs, rhs->getType());
			}

			if (lhs->getType()->isFloatingPointTy()) {
				switch (ban->op()) {
					case binary_arith_op::bxor:
					case binary_arith_op::band:
					case binary_arith_op::bor:
						lhs = builder.CreateBitCast(lhs, IntegerType::get(*llvm_context_, lhs->getType()->getPrimitiveSizeInBits())); break;
					default: break;
				}
			} else if (lhs->getType()->isVectorTy() && ((VectorType *)lhs->getType())->getElementType()->isFloatingPointTy()) {
				switch (ban->op()) {
					case binary_arith_op::bxor:
					case binary_arith_op::band:
					case binary_arith_op::bor: {
						auto ETy = ((VectorType *)lhs->getType())->getElementType();
						auto ENum = ((VectorType *)lhs->getType())->getElementCount();
						lhs = builder.CreateBitCast(lhs, VectorType::get(IntegerType::get(*llvm_context_, ETy->getPrimitiveSizeInBits()), ENum));
					} break;
					default: break;
				}
			}
			if (rhs->getType()->isFloatingPointTy()) {
				switch (ban->op()) {
					case binary_arith_op::bxor:
					case binary_arith_op::band:
					case binary_arith_op::bor:
						rhs = builder.CreateBitCast(lhs, IntegerType::get(*llvm_context_, rhs->getType()->getPrimitiveSizeInBits())); break;
					default: break;
				}
			} else if (rhs->getType()->isVectorTy() && ((VectorType *)rhs->getType())->getElementType()->isFloatingPointTy()) {
				switch (ban->op()) {
					case binary_arith_op::bxor:
					case binary_arith_op::band:
					case binary_arith_op::bor: {
						auto ETy = ((VectorType *)rhs->getType())->getElementType();
						auto ENum = ((VectorType *)rhs->getType())->getElementCount();
						rhs = builder.CreateBitCast(rhs, VectorType::get(IntegerType::get(*llvm_context_, ETy->getPrimitiveSizeInBits()), ENum));
					} break;
					default: break;
				}
			}

			bool is_f_or_fv = lhs->getType()->isFloatingPointTy() || (lhs->getType()->isVectorTy() && ((VectorType *)lhs->getType())->getElementType()->isFloatingPointTy());
			switch (ban->op()) {
			case binary_arith_op::bxor:
				return builder.CreateXor(lhs, rhs);
			case binary_arith_op::band:
				return builder.CreateAnd(lhs, rhs);
			case binary_arith_op::bor:
				return builder.CreateOr(lhs, rhs);
			case binary_arith_op::add: {
				if (lhs->getType()->isFloatingPointTy())
					return builder.CreateFAdd(lhs, rhs);
				return builder.CreateAdd(lhs, rhs);
			}
			case binary_arith_op::sub: {
				if (is_f_or_fv)
					return builder.CreateFSub(lhs, rhs);
				return builder.CreateSub(lhs, rhs);
			}
			case binary_arith_op::mul: {
				if (is_f_or_fv)
					return builder.CreateFMul(lhs, rhs);
				return builder.CreateMul(lhs, rhs);
			}
			case binary_arith_op::div: {
				if (is_f_or_fv)
					return builder.CreateFDiv(lhs, rhs);
				return builder.CreateUDiv(lhs, rhs);
			}
			case binary_arith_op::cmpeq: {
				if (lhs->getType()->isFloatingPointTy())
					return builder.CreateCmp(CmpInst::Predicate::FCMP_OEQ, lhs, rhs);
				return builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, lhs, rhs);
			}
			case binary_arith_op::cmpne: {
				if (lhs->getType()->isFloatingPointTy())
					return builder.CreateCmp(CmpInst::Predicate::FCMP_ONE, lhs, rhs);
				return builder.CreateCmp(CmpInst::Predicate::ICMP_NE, lhs, rhs);
			}
			case binary_arith_op::cmpgt: {
				// TODO: ordering for floats?
				if (lhs->getType()->isFloatingPointTy())
					return builder.CreateCmp(CmpInst::Predicate::FCMP_OGT, lhs, rhs);
				if (((IntegerType *)lhs->getType())->getSignBit())
					return builder.CreateCmp(CmpInst::Predicate::ICMP_SGT, lhs, rhs);
				else
					return builder.CreateCmp(CmpInst::Predicate::ICMP_UGT, lhs, rhs);
			}
			case binary_arith_op::mod: {
				auto ltype = lhs->getType();
				auto rtype = rhs->getType();
				if (ltype->isFloatingPointTy() && rtype->isFloatingPointTy())
					return builder.CreateFRem(lhs, rhs);
				if (((IntegerType *)ltype)->getSignBit() && ((IntegerType *)ltype)->getSignBit())
					return builder.CreateSRem(lhs, rhs);
				return builder.CreateURem(lhs, rhs);
			}
			case binary_arith_op::cmpo: {
				return builder.CreateCmp(CmpInst::FCMP_ORD, lhs, rhs);
			}
			case binary_arith_op::cmpu: {
				return builder.CreateCmp(CmpInst::FCMP_UNO, lhs, rhs);
			}
			case binary_arith_op::cmpoeq: {
				return builder.CreateCmp(CmpInst::FCMP_OEQ, lhs, rhs);
			}
			case binary_arith_op::cmpolt: {
				return builder.CreateCmp(CmpInst::FCMP_OLT, lhs, rhs);
			}
			case binary_arith_op::cmpole: {
				return builder.CreateCmp(CmpInst::FCMP_OLE, lhs, rhs);
			}
			case binary_arith_op::cmpueq: {
				return builder.CreateCmp(CmpInst::FCMP_UEQ, lhs, rhs);
			}
			case binary_arith_op::cmpult: {
				return builder.CreateCmp(CmpInst::FCMP_ULT, lhs, rhs);
			}
			case binary_arith_op::cmpune: {
				return builder.CreateCmp(CmpInst::FCMP_UNE, lhs, rhs);
			}
			case binary_arith_op::cmpunlt: {
				return builder.CreateCmp(CmpInst::FCMP_UGE, lhs, rhs);
			}
			case binary_arith_op::cmpunle: {
				return builder.CreateCmp(CmpInst::FCMP_UGT, lhs, rhs);
			}
			default:
				throw std::runtime_error("unsupported binary operator " + std::to_string((int)ban->op()));
			}
		} else if (p.kind() == port_kinds::zero) {
			switch (ban->op()) {
			case binary_arith_op::band:
			case binary_arith_op::mod:
			case binary_arith_op::bor:
			case binary_arith_op::bxor:
			case binary_arith_op::sub:
			case binary_arith_op::add: {
				auto value_port = lower_port(builder, state_arg, pkt, n->val());
				return builder.CreateZExt(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, value_port, ConstantInt::get(value_port->getType(), 0)), types.i8);
			}
			case binary_arith_op::cmpeq: {
				auto value_port = lower_port(builder, state_arg, pkt, n->val());
				return builder.CreateZExt(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, value_port, ConstantInt::get(value_port->getType(), 1)), types.i8);
			}
			default:
				throw std::runtime_error("unsupported binary operator for zero flag: " + std::to_string((int)ban->op()));
			}
		} else if (p.kind() == port_kinds::negative) {
			auto value_port = lower_port(builder, state_arg, pkt, n->val());
			return builder.CreateZExt(builder.CreateCmp(CmpInst::Predicate::ICMP_SLT, value_port, ConstantInt::get(value_port->getType(), 0)), types.i8);
		} else if (p.kind() == port_kinds::carry) {
			auto value_port = lower_port(builder, state_arg, pkt, n->val());
			switch (ban->op()) {
			case binary_arith_op::sub: return builder.CreateZExt(builder.CreateICmpULT(lhs, rhs, "borrow"), types.i8);
			case binary_arith_op::add: return builder.CreateZExt(builder.CreateICmpUGT(lhs, value_port, "carry"), types.i8);
			default: return ConstantInt::get(types.i8, 0);
			}
		} else if (p.kind() == port_kinds::overflow) {
			auto value_port = lower_port(builder, state_arg, pkt, n->val());
			switch (ban->op()) {
			case binary_arith_op::sub: {
				auto z = ConstantInt::get(value_port->getType(), 0);
				rhs = builder.CreateNeg(rhs);
				auto sum = builder.CreateAdd(lhs, rhs);
				auto pos_args = builder.CreateAnd(builder.CreateICmpSGE(lhs, z, "LHS pos"), builder.CreateAnd(builder.CreateICmpSGE(rhs, z, "RHS pos"), builder.CreateICmpSLT(sum, z, "SUM neg")));
				auto neg_args = builder.CreateAnd(builder.CreateICmpSLT(lhs, z, "LHS neg"), builder.CreateAnd(builder.CreateICmpSLT(rhs, z, "RHS neg"), builder.CreateICmpSGE(sum, z, "SUM pos")));
				return builder.CreateZExt(builder.CreateOr(pos_args, neg_args, "overflow"), types.i8);
			}
			case binary_arith_op::add: {
				auto z = ConstantInt::get(value_port->getType(), 0);
				auto pos_args = builder.CreateAnd(builder.CreateICmpSGT(lhs, z), builder.CreateAnd(builder.CreateICmpSGT(rhs, z), builder.CreateICmpSLT(value_port, z)));
				auto neg_args = builder.CreateAnd(builder.CreateICmpSLT(lhs, z), builder.CreateAnd(builder.CreateICmpSLT(rhs, z), builder.CreateICmpSGE(value_port, z)));
				return builder.CreateZExt(builder.CreateOr(pos_args, neg_args, "overflow"), types.i8);
			}
			default: return ConstantInt::get(types.i8, 0);
			}
		} else {
			throw std::runtime_error("unsupported port kind");
		}
	}

	case node_kinds::read_pc: {
		// auto src_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, 0) }, "pcptr");
		// return builder.CreateLoad(types.i64, src_reg);
		Value *val = ConstantInt::get(types.i64, pkt->address());
		if (!e_.is_exec()) {
			Value *gvar = module_->getOrInsertGlobal("guest_base", types.i8);
			val = builder.CreateGEP(types.i8, gvar, val);
			val = builder.CreatePtrToInt(val, types.i64);
		}

		return val;
	}

	case node_kinds::cast: {
		auto cn = (cast_node *)n;

		auto val = lower_port(builder, state_arg, pkt, cn->source_value());

		::llvm::Type *ty;
		switch (cn->op()) {
		case cast_op::zx:
			switch (cn->target_type().width()) {
			case 8:
				ty = types.i8;
				break;
			case 16:
				ty = types.i16;
				break;
			case 32:
				ty = types.i32;
				break;
			case 64:
				ty = types.i64;
				break;
			case 128:
				ty = types.i128;
				break;
			case 256:
				ty = types.i256;
				break;
			case 512:
				ty = types.i512;
				break;

			default:
				throw std::runtime_error("unsupported zx width " + std::to_string(cn->val().type().width()));
			}
			// Truncating is correct if we come from a MOVD xmm -> m32, no need for bit extracts in the IR
			return builder.CreateZExtOrTrunc(val, ty, "zx");

		case cast_op::sx:
			switch (cn->val().type().width()) {
			case 16:
				ty = types.i16;
				break;
			case 32:
				ty = types.i32;
				break;
			case 64:
				ty = types.i64;
				break;
			case 128:
				ty = types.i128;
				break;
			default:
				throw std::runtime_error("unsupported sx width");
			}
			return builder.CreateSExt(val, ty);

		case cast_op::trunc:
			switch (cn->val().type().width()) {
			case 1:
				ty = types.i1;
				break;
			case 8:
				ty = types.i8;
				break;
			case 16:
				ty = types.i16;
				break;
			case 32:
				ty = types.i32;
				break;
			case 64:
				ty = types.i64;
				break;
			default:
				throw std::runtime_error("unsupported trunc width");
			}
			return builder.CreateTrunc(val, ty);

		case cast_op::bitcast: {
			if (cn->target_type().is_vector()) {
				switch (cn->target_type().element_width()) {
				case 1:
					ty = types.i1;
					break;
				case 8:
					ty = types.i8;
					break;
				case 16:
					ty = types.i16;
					break;
				case 32: {
					if (cn->target_type().is_floating_point())
						ty = types.f32;
					else
						ty = types.i32;
					break;
				}
				case 64: {
					if (cn->target_type().is_floating_point())
						ty = types.f64;
					else
						ty = types.i64;
					break;
				}
				case 128: {
						ty = types.i128;
					break;
				}
				default:
					throw std::runtime_error("unsupported bitcast vector element width");
				}
				return builder.CreateBitCast(val, ::llvm::VectorType::get(ty, cn->target_type().nr_elements(), false));
			}
			//if (val->getType()->isVectorTy()) {
				switch (cn->target_type().width()) {
				case 1:
					ty = types.i1;
					break;
				case 8:
					ty = types.i8;
					break;
				case 16:
					ty = types.i16;
					break;
				case 32: {
					if (cn->target_type().is_floating_point())
						ty = types.f32;
					else
						ty = types.i32;
					break;
				}
				case 64: {
					if (cn->target_type().is_floating_point())
						ty = types.f64;
					else
						ty = types.i64;
					break;
				}
				case 128:
					ty = types.i128;
					break;
				case 256:
					ty = types.i256;
					break;
				case 512:
					ty = types.i512;
					break;
				default:
					throw std::runtime_error("unsupported bitcast element width");
				}
				return builder.CreateBitCast(val, ty);

			//}
			//return val;
		}
		case cast_op::convert: {
			::llvm::RoundingMode rm;
			switch (cn->convert_type()) {
				case fp_convert_type::round: rm = ::llvm::RoundingMode::NearestTiesToEven; break;
				case fp_convert_type::trunc: rm = ::llvm::RoundingMode::TowardZero; break;
			}

			if (cn->target_type().is_floating_point()) {
				switch (cn->target_type().width()) {
					case 32: ty = types.f32; break;
					case 64: ty = types.f64; break;
					case 80: ty = types.f80; break;
				}
				if (val->getType()->isFloatingPointTy()) {
					if (val->getType()->getPrimitiveSizeInBits() > ty->getPrimitiveSizeInBits())
						return builder.CreateFPTrunc(val, ty);
					return builder.CreateFPExt(val, ty);
				}

				//TODO: Valid rounding mode!
				if (((IntegerType *)val->getType())->getSignBit())
					return builder.CreateConstrainedFPCast(Intrinsic::experimental_constrained_sitofp, val, ty);
				return builder.CreateConstrainedFPCast(Intrinsic::experimental_constrained_uitofp, val, ty);
			}
			switch (cn->target_type().width()) {
				case 1: ty = types.i1; break;
				case 8: ty = types.i8; break;
				case 16: ty = types.i16; break;
				case 32: ty = types.i32; break;
				case 64: ty = types.i64; break;
				case 128: ty = types.i128; break;
				case 256: ty = types.i256; break;
				case 512: ty = types.i512; break;
			}
			return builder.CreateFPToSI(val, ty);
							   }
		default:
			throw std::runtime_error("unsupported cast op");
		}
	}

	case node_kinds::csel: {
		auto cn = (csel_node *)n;

		auto cond = lower_port(builder, state_arg, pkt, cn->condition());
		auto tv = lower_port(builder, state_arg, pkt, cn->trueval());
		auto fv = lower_port(builder, state_arg, pkt, cn->falseval());

		return builder.CreateSelect(cond, tv, fv, "csel");
	}

	case node_kinds::unary_arith: {
		auto un = (unary_arith_node *)n;

		auto v = lower_port(builder, state_arg, pkt, un->lhs());
		if (v->getType()->isFloatingPointTy() && un->op() != unary_arith_op::sqrt)
			v = builder.CreateBitCast(v, IntegerType::getIntNTy(*llvm_context_, v->getType()->getPrimitiveSizeInBits()));
		if (v->getType()->isVectorTy() && v->getType()->isFPOrFPVectorTy()) {
			auto ETy = ((VectorType *)v->getType())->getElementType();
			auto ENum  = ((VectorType *)v->getType())->getElementCount();
			auto DstTy = VectorType::get(IntegerType::get(*llvm_context_, ETy->getPrimitiveSizeInBits()), ENum);
			v = builder.CreateBitCast(v, DstTy);
		}

		switch (p.kind()) {
		case port_kinds::value: {

			switch (un->op()) {
			case unary_arith_op::bnot:
				return builder.CreateNot(v);
			case unary_arith_op::sqrt:
				return builder.CreateUnaryIntrinsic(Intrinsic::sqrt, v);
			default:
				throw std::runtime_error("unsupported unary operator");
			}
		}
		case port_kinds::zero: return builder.CreateZExt(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, v, ConstantInt::get(v->getType(), 0)), types.i8);
		case port_kinds::negative: return builder.CreateZExt(builder.CreateCmp(CmpInst::Predicate::ICMP_SLT, v, ConstantInt::get(v->getType(), 0)), types.i8);
		default: return ConstantInt::get(types.i8, 0);
		}
	}

	case node_kinds::bit_shift: {
		auto bsn = (bit_shift_node *)n;
		auto input = lower_port(builder, state_arg, pkt, bsn->input());
		auto amount = lower_port(builder, state_arg, pkt, bsn->amount());

		if (p.kind() == port_kinds::value) {
			amount = builder.CreateZExtOrTrunc(amount, input->getType());

			switch (bsn->op()) {
			case shift_op::asr:
				return builder.CreateAShr(input, amount, "bit_shift");
			case shift_op::lsr:
				return builder.CreateLShr(input, amount, "bit_shift");
			case shift_op::lsl:
				return builder.CreateShl(input, amount, "bit_shift");

			default:
				throw std::runtime_error("unsupported shift op");
			}
		} else if (p.kind() == port_kinds::zero) {
			auto value_port = lower_port(builder, state_arg, pkt, n->val());
			return builder.CreateZExt(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, value_port, ConstantInt::get(value_port->getType(), 0)), types.i8);
		} else if (p.kind() == port_kinds::negative) {
			auto value_port = lower_port(builder, state_arg, pkt, n->val());
			return builder.CreateZExt(builder.CreateCmp(CmpInst::Predicate::ICMP_SLT, value_port, ConstantInt::get(value_port->getType(), 0)), types.i8);
		} else if (p.kind() == port_kinds::carry) {
			auto input_bits = builder.CreateBitCast(input, VectorType::get(types.i1, input->getType()->getIntegerBitWidth(), false));
			switch (bsn->op()) {
				case shift_op::asr:
				case shift_op::lsr: {
					return builder.CreateZExt(builder.CreateExtractVector(types.i1, input_bits, builder.CreateSub(amount, ConstantInt::get(amount->getType(), 1))), types.i8);
				}
				case shift_op::lsl: {
					return builder.CreateZExt(builder.CreateExtractVector(types.i1, input_bits, builder.CreateSub(ConstantInt::get(amount->getType(), input->getType()->getIntegerBitWidth()), amount)), types.i8);
				}
			}
		} else if (p.kind() == port_kinds::overflow) {
			// asusming there are no 0 shifts
			auto input_bits = builder.CreateBitCast(input, VectorType::get(types.i1, input->getType()->getIntegerBitWidth(), false));
			auto msb = builder.CreateExtractVector(types.i1, input_bits, builder.CreateSub(ConstantInt::get(types.i64, input->getType()->getIntegerBitWidth()), ConstantInt::get(types.i64, 1)));
			auto smsb = builder.CreateExtractVector(types.i1, input_bits, builder.CreateSub(ConstantInt::get(types.i64, input->getType()->getIntegerBitWidth()), ConstantInt::get(types.i64, 2)));
			auto undefined_or_cleared =  ConstantInt::get(types.i8, 0);
			switch(bsn->op()) {
				case shift_op::lsl: return builder.CreateZExt(builder.CreateXor(msb, smsb), types.i8);
				case shift_op::lsr: return builder.CreateZExt(msb, types.i8);
				default: return undefined_or_cleared;
			}
		}
	}

	case node_kinds::ternary_arith: {
		auto tan = (ternary_arith_node *)n;

		if (p.kind() == port_kinds::value) {
			auto lhs = lower_port(builder, state_arg, pkt, tan->lhs());
			auto rhs = lower_port(builder, state_arg, pkt, tan->rhs());
			auto top = lower_port(builder, state_arg, pkt, tan->top());

			switch (tan->op()) {
			case ternary_arith_op::adc:
				return builder.CreateAdd(lhs, builder.CreateAdd(rhs, top));

			case ternary_arith_op::sbb:
				return builder.CreateSub(lhs, builder.CreateAdd(rhs, top));

			default:
				throw std::runtime_error("unsupported ternary op");
			}
		} else {
			// TODO: Flags
			return ConstantInt::get(types.i8, 0);
		}
	}

	case node_kinds::bit_extract: {
		auto uncle = (bit_extract_node *)n;
		auto val = lower_port(builder, state_arg, pkt, uncle->source_value());
/*
		auto val_bit = builder.CreateBitCast(val, VectorType::get(types.i1, val->getType()->getIntegerBitWidth(), false));

		auto result = builder.CreateExtractVector(VectorType::get(types.i1, uncle->length(), false), val_bit, ConstantInt::get( types.i64, uncle->from()));
		return builder.CreateBitCast(result, Type::getIntNTy(*llvm_context_, uncle->length()));
*/
		auto tmp = builder.CreateShl(val, ConstantInt::get(val->getType(), val->getType()->getPrimitiveSizeInBits() - (uncle->from()+uncle->length()) ));
		tmp = builder.CreateAShr(tmp, ConstantInt::get(val->getType(), val->getType()->getPrimitiveSizeInBits() - uncle->length()) );
		return builder.CreateTruncOrBitCast(tmp, IntegerType::getIntNTy(*llvm_context_, uncle->length()));
	}
	case node_kinds::bit_insert: {
		auto bin = (bit_insert_node *)n;
		auto dst = lower_port(builder, state_arg, pkt, bin->source_value());
		auto val = lower_port(builder, state_arg, pkt, bin->bits());
		auto dst_ty = dst->getType();

		if (dst_ty->isFloatingPointTy())
			dst = builder.CreateBitCast(dst, IntegerType::get(*llvm_context_, dst_ty->getPrimitiveSizeInBits()));

		auto tmp = ConstantInt::get(dst->getType(), 1);
		auto ones = builder.CreateSub(builder.CreateShl(tmp, ConstantInt::get(tmp->getType(), bin->length())), ConstantInt::get(tmp->getType(), 1));
		auto mask = builder.CreateShl(ones, ConstantInt::get(tmp->getType(), bin->to()), "bit_insert gen mask");
		auto inv_mask = builder.CreateXor(mask, ConstantInt::get(mask->getType(), -1), "Neg mask");

		auto insert = builder.CreateAnd(
			builder.CreateShl(
				builder.CreateZExtOrTrunc(
					builder.CreateBitCast(val, IntegerType::getIntNTy(*llvm_context_, val->getType()->getPrimitiveSizeInBits())),
					IntegerType::getIntNTy(*llvm_context_, dst->getType()->getPrimitiveSizeInBits())),
				ConstantInt::get(dst->getType(), bin->to())),
			mask, "bit_insert apply mask");
		auto out = builder.CreateAnd(dst, inv_mask, "bit_insert mask out");
		out = builder.CreateOr(out, insert);
		if (dst_ty->isFloatingPointTy())
			return builder.CreateBitCast(out, dst_ty);
		return out;
	}

        case node_kinds::vector_insert: {
			return lower_node(builder, state_arg, pkt, n);
        }

        case node_kinds::vector_extract: {
			return lower_node(builder, state_arg, pkt, n);
        }

	case node_kinds::binary_atomic: {
		auto ban = (binary_atomic_node *)n;
		if (p.kind() == port_kinds::value)
			return lower_node(builder, state_arg, pkt, n);

		auto rhs = lower_port(builder, state_arg, pkt, ban->rhs());
		auto lhs = lower_port(builder, state_arg, pkt, ban->val());
#ifndef ARCH_X86_64
		//auto gs_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, 26) }); //TODO: move offset_2_idx into a common header
		auto gs_reg = reg_to_alloca_.at(reg_offsets::GS);
		//lhs = builder.CreateAdd(lhs, builder.CreateLoad(types.i64, gs_reg));
#endif
		auto value_port = lower_port(builder, state_arg, pkt, ban->operation_value());

		if (p.kind() == port_kinds::zero) {
			return builder.CreateZExt(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, value_port, ConstantInt::get(value_port->getType(), 0)), types.i8);
		} else if (p.kind() == port_kinds::negative) {
			return builder.CreateZExt(builder.CreateCmp(CmpInst::Predicate::ICMP_SLT, value_port, ConstantInt::get(value_port->getType(), 0)), types.i8);
		} else if (p.kind() == port_kinds::carry) {
			switch (ban->op()) {
			case binary_atomic_op::sub: return builder.CreateZExt(builder.CreateICmpULT(lhs, rhs, "borrow"), types.i8);
			case binary_atomic_op::xadd:
			case binary_atomic_op::add: return builder.CreateZExt(builder.CreateICmpUGT(lhs, value_port, "carry"), types.i8);
			default: return ConstantInt::get(types.i8, 0);
			}
		} else if (p.kind() == port_kinds::overflow) {
			switch (ban->op()) {
			case binary_atomic_op::sub: {
				auto z = ConstantInt::get(value_port->getType(), 0);
				auto sum = builder.CreateAdd(lhs, builder.CreateNeg(rhs));
				auto pos_args = builder.CreateAnd(builder.CreateICmpSGT(lhs, z), builder.CreateAnd(builder.CreateICmpSGT(rhs, z), builder.CreateICmpSLT(sum, z)));
				auto neg_args = builder.CreateAnd(builder.CreateICmpSLT(lhs, z), builder.CreateAnd(builder.CreateICmpSLT(rhs, z), builder.CreateICmpSGE(sum, z)));
				return builder.CreateZExt(builder.CreateOr(pos_args, neg_args, "overflow"), types.i8);
			}
			case binary_atomic_op::xadd:
			case binary_atomic_op::add: {
				auto z = ConstantInt::get(value_port->getType(), 0);
				auto pos_args = builder.CreateAnd(builder.CreateICmpSGT(lhs, z), builder.CreateAnd(builder.CreateICmpSGT(rhs, z), builder.CreateICmpSLT(value_port, z)));
				auto neg_args = builder.CreateAnd(builder.CreateICmpSLT(lhs, z), builder.CreateAnd(builder.CreateICmpSLT(rhs, z), builder.CreateICmpSGE(value_port, z)));
				return builder.CreateZExt(builder.CreateOr(pos_args, neg_args, "overflow"), types.i8);
			}
			default: return ConstantInt::get(types.i8, 0);
			}
		return ConstantInt::get(types.i8, 0);
		}
	}
	case node_kinds::ternary_atomic: {
        auto tan = (ternary_atomic_node *)n;
		if (p.kind() == port_kinds::value)
			return lower_node(builder, state_arg, pkt, n);
		if (p.kind() == port_kinds::zero) {
            auto lhs = lower_port(builder, state_arg, pkt, tan->address());
            auto rhs = lower_port(builder, state_arg, pkt, tan->rhs());
            auto top = lower_port(builder, state_arg, pkt, tan->top());

			lhs = builder.CreateIntToPtr(lhs, PointerType::get(rhs->getType(), 256));
            lhs = builder.CreateLoad(rhs->getType(), lhs);
			return builder.CreateZExt(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, top, lhs), types.i8);

        }
        // TODO: Flags
		return ConstantInt::get(types.i8, 0);
	}
	case node_kinds::read_local: {
	        auto rln = (read_local_node *)n;
	        auto address = local_var_to_llvm_addr_.at(rln->local());
	        ::llvm::Type *ty;
	        switch (rln->local()->type().width()) {
					case 8: ty = types.i8; break;
	                case 80: ty = types.f80; break;
	                case 32: {
						if (rln->local()->type().is_integer())
							ty = types.i32;
						else
							ty = types.f32;
					} break;
	                case 64: {
						if (rln->local()->type().is_integer())
							ty = types.i64;
						else
							ty = types.f64;
					} break;
					default: throw std::runtime_error("unsupported read_local width: "+std::to_string(rln->local()->type().width()));
	        }
	        auto load = builder.CreateLoad(ty, address, "read_local");
	        return load;
	}
	case node_kinds::internal_call: {
		if (p.kind() == port_kinds::value)
			return lower_node(builder, state_arg, pkt, n);
	}
	default:
		throw std::runtime_error("materialize_port: unsupported port node kind " + std::to_string((int)n->kind()));
	}
}

Value *llvm_static_output_engine_impl::lower_port(IRBuilder<> &builder, Argument *state_arg, std::shared_ptr<packet> pkt, port &p)
{
	auto existing = node_ports_to_llvm_values_.find(&p);

	if (existing != node_ports_to_llvm_values_.end()) {
		return existing->second;
	}

	Value *v = materialise_port(builder, state_arg, pkt, p);
	node_ports_to_llvm_values_[&p] = v;

	return v;
}

Value *llvm_static_output_engine_impl::lower_node(IRBuilder<> &builder, Argument *state_arg, std::shared_ptr<packet> pkt, node *a)
{
	switch (a->kind()) {
	case node_kinds::label: {
		auto ln = (label_node *)a;
		auto current_block = builder.GetInsertBlock();

		auto intermediate_block = label_nodes_to_llvm_blocks_[ln];
		if (!intermediate_block) {
			intermediate_block = BasicBlock::Create(*llvm_context_, "LABEL-"+ln->name(), current_block->getParent());
			label_nodes_to_llvm_blocks_[ln] = intermediate_block;
		}
		if (!in_br) {
			builder.CreateBr(intermediate_block);
			builder.SetInsertPoint(intermediate_block);
		}

		return nullptr;
	}

	case node_kinds::write_reg: {
		auto wrn = (write_reg_node *)a;
		// gep register
		// store value

		// std::cerr << "wreg name=" << wrn->regname() << std::endl;

		//auto dest_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, wrn->regidx()) },
		//	idx_to_reg_name(wrn->regidx()));
		auto dest_reg = reg_to_alloca_.at((reg_offsets)wrn->regoff());

		//auto *reg_type = ((GetElementPtrInst*)dest_reg)->getResultElementType();
		auto *reg_type = ((AllocaInst *)dest_reg)->getAllocatedType();

		auto val = lower_port(builder, state_arg, pkt, wrn->value());

		//Bitcast the resulting value to the type of the register
		//since the operations can happen on different type after
		//other casting operations
		if(val->getType()->isVectorTy())
			val = builder.CreateBitCast(val, IntegerType::get(*llvm_context_, val->getType()->getPrimitiveSizeInBits()));
		auto reg_val = builder.CreateZExtOrBitCast(val, reg_type);

		Align align = Align(8);
		switch (reg_type->getPrimitiveSizeInBits()) {
		case 1:
		case 8: {
			align = Align(1);
			break;
				}
		case 16:
		case 32:
		case 64:
			break;
		case 128:
		case 256:
		case 512: {
			align = Align(64);
			break;
				  }
		default:
			throw std::runtime_error("unsupported register width " + std::to_string(wrn->val().type().width()) + " in store");
		}
		return builder.CreateAlignedStore(reg_val, dest_reg, align);
	}

	case node_kinds::write_mem: {
		auto wmn = (write_mem_node *)a;

		auto address = lower_port(builder, state_arg, pkt, wmn->address());
		auto value = lower_port(builder, state_arg, pkt, wmn->value());

#ifndef ARCH_X86_64
		//auto gs_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, 26) }); //TODO: move offset_2_idx into a common header
		auto gs_reg = reg_to_alloca_.at(reg_offsets::GS);
		//address = builder.CreateAdd(address, builder.CreateLoad(types.i64, gs_reg));
#endif
		auto address_ptr = builder.CreateIntToPtr(address, PointerType::get(value->getType(), 256));

		if (e_.fences_) {
			if (!is_stack(wmn))
				builder.CreateFence(AtomicOrdering::Release);
		}
		auto store = builder.CreateStore(value, address_ptr);
		store->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(store->getContext(), guest_mem_alias_scope_));
		store->setMetadata(LLVMContext::MD_noalias, MDNode::get(store->getContext(), reg_file_alias_scope_));
		return store;
	}

	case node_kinds::write_pc: {
		auto wpn = (write_pc_node *)a;

		auto dest_mem = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, 0) }, "pcptr");
		auto dest_reg = reg_to_alloca_.at(reg_offsets::PC);
		auto val = lower_port(builder, state_arg, pkt, wpn->value());

		// For Debug only! This will break the static_csel pass
		// builder.CreateStore(val, dest_mem);
		return builder.CreateStore(val, dest_reg);
	}

	case node_kinds::cond_br: {
		auto cbn = (cond_br_node *)a;

		auto current_block = builder.GetInsertBlock();
		auto intermediate_block = BasicBlock::Create(*llvm_context_, "Cond not taken", current_block->getParent());

		auto cond = lower_port(builder, state_arg, pkt, cbn->cond());
		in_br = true;
		lower_node(builder, state_arg, pkt, cbn->target());
		in_br = false;

		auto br = builder.CreateCondBr(cond, label_nodes_to_llvm_blocks_[cbn->target()], intermediate_block);

		builder.SetInsertPoint(intermediate_block);

		return br;
		}
		case node_kinds::br: {
	        auto bn = (br_node *)a;

			auto current_block = builder.GetInsertBlock();
			auto intermediate_block = BasicBlock::Create(*llvm_context_, "br next", current_block->getParent());

			in_br = true;
			lower_node(builder, state_arg, pkt, bn->target());
			in_br = false;
			auto br = builder.CreateBr(label_nodes_to_llvm_blocks_[bn->target()]);

			builder.SetInsertPoint(intermediate_block);

			return br;
		}
        case node_kinds::vector_insert: {
                auto vin = (vector_insert_node *)a;

                auto vec = lower_port(builder, state_arg, pkt, vin->source_vector());
                auto val = lower_port(builder, state_arg, pkt, vin->insert_value());

				if (!vec->getType()->isVectorTy()) {
					auto v_len = vec->getType()->getPrimitiveSizeInBits();
					vec = builder.CreateBitCast(vec, VectorType::get(val->getType(), v_len/val->getType()->getPrimitiveSizeInBits(), false));
				}

				auto ve_type = ((VectorType *)(vec->getType()))->getElementType();
				if (ve_type->isFloatingPointTy() && !val->getType()->isFloatingPointTy()) {
					switch(ve_type->getPrimitiveSizeInBits()) {
						case 32: val = builder.CreateBitCast(val, types.f32); break;
						case 64: val = builder.CreateBitCast(val, types.f64); break;
						case 80: val = builder.CreateBitCast(val, types.f80); break;
					}
				}

                auto insert = builder.CreateInsertElement(vec, val, vin->index(), "vector_insert");

                return insert;
        }

        case node_kinds::vector_extract: {
                auto ven = (vector_extract_node *)a;

                auto vec = lower_port(builder, state_arg, pkt, ven->source_vector());

                auto extract = builder.CreateExtractElement(vec, ven->index(), "vector_extract");

                return extract;
        }

	case node_kinds::binary_atomic: {
		auto ban = (binary_atomic_node *)a;
		auto lhs = lower_port(builder, state_arg, pkt, ban->address());
#ifndef ARCH_X86_64
		//auto gs_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, 26) }); //TODO: move offset_2_idx into a common header
		auto gs_reg = reg_to_alloca_.at(reg_offsets::GS);
		//lhs = builder.CreateAdd(lhs, builder.CreateLoad(types.i64, gs_reg));
#endif
		auto rhs = lower_port(builder, state_arg, pkt, ban->rhs());

		auto existing = node_ports_to_llvm_values_.find(&ban->val());
		if (existing != node_ports_to_llvm_values_.end())
			return existing->second;

		lhs = builder.CreateIntToPtr(lhs, PointerType::get(rhs->getType(), 256));
		if (e_.fences_) {
			builder.CreateFence(AtomicOrdering::SequentiallyConsistent);
		}
		AtomicRMWInst *out;
		Value *val;
		switch (ban->op()) {
		case binary_atomic_op::band:
			out = builder.CreateAtomicRMW(AtomicRMWInst::And, lhs, rhs, Align(1), AtomicOrdering::SequentiallyConsistent);
			val = builder.CreateAnd(out, rhs);
			break;
		case binary_atomic_op::add:
			out = builder.CreateAtomicRMW(AtomicRMWInst::Add, lhs, rhs, Align(1), AtomicOrdering::SequentiallyConsistent);
			val = builder.CreateAdd(out, rhs);
			break;
		case binary_atomic_op::sub:
			out = builder.CreateAtomicRMW(AtomicRMWInst::Sub, lhs, rhs, Align(1), AtomicOrdering::SequentiallyConsistent);
			val = builder.CreateSub(out, rhs);
			break;
		case binary_atomic_op::xadd:
			//			out = builder.CreateAtomicRMW(AtomicRMWInst::Add, lhs, rhs, Align(1), AtomicOrdering::SequentiallyConsistent);
			throw std::runtime_error("Should not happen");
			break;
		case binary_atomic_op::bor:
			out = builder.CreateAtomicRMW(AtomicRMWInst::Or, lhs, rhs, Align(1), AtomicOrdering::SequentiallyConsistent);
			val = builder.CreateOr(out, rhs);
			break;
		case binary_atomic_op::xchg:
			out = builder.CreateAtomicRMW(AtomicRMWInst::Xchg, lhs, rhs, Align(1), AtomicOrdering::SequentiallyConsistent);
			val = rhs;
			break;
		default:
			throw std::runtime_error("unsupported bin atomic operation " + std::to_string((int)ban->op()));
		}
		//		if (out) {
		//			switch (ban->op()) {
		//			// case binary_atomic_op::xadd: out = builder.CreateAtomicRMW(AtomicRMWInst::Xchg, lhs, out, Align(64),
		//AtomicOrdering::SequentiallyConsistent);
		//			// break;
		//			default:
		//				break;
		//			}
		if (e_.fences_) {
			builder.CreateFence(AtomicOrdering::SequentiallyConsistent);
		}
		node_ports_to_llvm_values_[&ban->val()] = out;
		node_ports_to_llvm_values_[&ban->operation_value()] = val;
		return out;
		//		}
		//		auto ret = builder.CreateLoad(rhs->getType(), builder.CreateIntToPtr(lhs, PointerType::get(rhs->getType(), 256)), "Atomic result");
		//		builder.CreateFence(AtomicOrdering::SequentiallyConsistent);
		//		node_ports_to_llvm_values_[&ban->val()] = ret;
		//		return ret;
	}
	case node_kinds::ternary_atomic: {
		auto tan = (ternary_atomic_node *)a;
		auto lhs = lower_port(builder, state_arg, pkt, tan->address());
#ifndef ARCH_X86_64
		//auto gs_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, 26) }); //TODO: move offset_2_idx into a common header
		auto gs_reg = reg_to_alloca_.at(reg_offsets::GS);
		//lhs = builder.CreateAdd(lhs, builder.CreateLoad(types.i64, gs_reg));
#endif
		auto rhs = lower_port(builder, state_arg, pkt, tan->rhs());
		auto top = lower_port(builder, state_arg, pkt, tan->top());
		auto rax_node = tan->rhs().owner();
		assert((rax_node->kind() == node_kinds::read_reg) || "Cmpxcg[top] is not a register");
		auto reg_idx = ((read_reg_node *)rax_node)->regidx();
		//auto rax_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, reg_idx) });
		auto rax_reg = reg_to_alloca_.at(reg_offsets::RAX);

		Value *out;
		switch(tan->op()) {
			case ternary_atomic_op::cmpxchg: {

                Align align;
                switch (rhs->getType()->getPrimitiveSizeInBits()) {
                    case 1:
                    case 8: align = Align(1); break;
                    case 16: align = Align(2); break;
                    case 32: align = Align(4); break;
                    case 64: align = Align(8); break;
                }
				lhs = builder.CreateIntToPtr(lhs, PointerType::get(rhs->getType(), 256));
				if (e_.fences_) {
					//builder.CreateFence(AtomicOrdering::SequentiallyConsistent);
				}
				auto instr = builder.CreateAtomicCmpXchg(lhs, rhs, top, align, AtomicOrdering::SequentiallyConsistent, AtomicOrdering::SequentiallyConsistent);
				//auto new_rax_val = builder.CreateSelect(builder.CreateExtractValue(instr, 1), rhs, builder.CreateExtractValue(instr, 0));
				auto new_rax_val = builder.CreateExtractValue(instr, 0);
				builder.CreateStore(builder.CreateZExt(new_rax_val, types.i64), rax_reg);
				if (e_.fences_) {
					//builder.CreateFence(AtomicOrdering::SequentiallyConsistent);
				}

				return instr;
			}
			default: throw std::runtime_error("unsupported tern atomic operation " + std::to_string((int)tan->op()));
		}
		return out;
	}
	case node_kinds::internal_call: {
		auto icn = (internal_call_node *)a;

		auto existing = node_ports_to_llvm_values_.find(&icn->val());
		if (existing != node_ports_to_llvm_values_.end())
			return existing->second;


		auto switch_callee = module_->getOrInsertFunction("execute_internal_call", types.internal_call_handler);
		if (icn->fn().name() == "handle_syscall") {

			auto clk_ = module_->getOrInsertFunction("clk", types.clk_fn);

			auto current_bb = builder.GetInsertBlock();
			auto exit_block = BasicBlock::Create(*llvm_context_, "finalize",  current_bb->getParent());
			auto cont_block = BasicBlock::Create(*llvm_context_, "cont",  current_bb->getParent());
			save_all_regs(builder, state_arg);
#if defined(DEBUG)
			builder.CreateCall(clk_, {state_arg, builder.CreateGlobalStringPtr("do-syscall")});
#endif
			auto ret = builder.CreateCall(switch_callee, { state_arg, ConstantInt::get(types.i32, 1) });
#if defined(DEBUG)
			builder.CreateCall(clk_, {state_arg, builder.CreateGlobalStringPtr("done-syscall")});
#endif
			restore_all_regs(builder, state_arg);
			builder.CreateCondBr(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, ret, ConstantInt::get(types.i32, 1)), exit_block, cont_block);

			builder.SetInsertPoint(exit_block);
			auto finalize = module_->getOrInsertFunction("finalize", types.finalize);
			auto call = builder.CreateCall(finalize, {});
			call->setDoesNotReturn();
			builder.CreateBr(cont_block);

			builder.SetInsertPoint(cont_block);
			return ret;
		} else if (icn->fn().name() == "handle_poison") {
			auto name = ((label_node *)icn->args()[0]->owner())->name();
			auto ptr = builder.CreateGlobalStringPtr(name);

			auto pf = module_->getOrInsertFunction("poison", types.poison_fn);
			return builder.CreateCall(pf, { ptr });
		} else if (icn->fn().name() == "hlt") {
			return builder.CreateCall(switch_callee, { state_arg, ConstantInt::get(types.i32, 3) });
		} else if (icn->fn().name() == "handle_int") {
			return builder.CreateCall(switch_callee, { state_arg, ConstantInt::get(types.i32, 2) });
		} else {
			const port &ret = icn->val();
			const std::vector<port *> &args = icn->args();
			const internal_function &func = icn->fn();

			Type *retty;
			if (ret.type().is_floating_point()) {
				if (ret.type().element_width() == 32) {
					retty = types.f32;
				} else if (ret.type().element_width() == 64) {
					retty = types.f64;
				}
			} else if (ret.type().is_integer()) {
				switch (ret.type().width()) {
				case 1:
					retty = types.i1;
					break;
				case 8:
					retty = types.i8;
					break;
				case 16:
					retty = types.i16;
					break;
				case 32:
					retty = types.i32;
					break;
				case 64:
					retty = types.i64;
					break;
				default:
					throw std::runtime_error("unsupported register width " + std::to_string(ret.type().width()) + " in internal_call return");
				}
			} else if (ret.type().type_class() == value_type_class::none) {
				retty = types.vd;
			}
			std::vector<Type *> argtys;
			std::vector<Value *> arg_vals;

			for (const auto arg : args) {
				auto arg_val = lower_port(builder, state_arg, pkt, *arg);
				argtys.push_back(arg_val->getType());
				arg_vals.push_back(arg_val);
			}

			FunctionType *ftype = FunctionType::get(retty, argtys, false);
			const FunctionCallee &ext_func = module_->getOrInsertFunction(func.name(), ftype);

			CallInst *out = builder.CreateCall(ext_func, arg_vals);

			node_ports_to_llvm_values_[&icn->val()] = out;

			return out;
		}
	}
	case node_kinds::write_local: {
        auto wln = (write_local_node *)a;
        auto val = lower_port(builder, state_arg, pkt, wln->write_value());
        ::llvm::Type *ty;
        switch (wln->write_value().type().width()) {
                case 8: ty = types.i8; break;
                case 80: ty = types.f80; break;
	            case 32: {
					if (wln->local()->type().is_integer())
						ty = types.i32;
					else
						ty = types.f32;
				} break;
                case 64: {
						if (wln->local()->type().is_integer())
							ty = types.i64;
						else
							ty = types.f64;
				} break;
				default: throw std::runtime_error("unsupported alloca width: " + std::to_string(wln->write_value().type().width()));
        }
		if (local_var_to_llvm_addr_.find(wln->local()) == local_var_to_llvm_addr_.end())
			local_var_to_llvm_addr_[wln->local()] = builder.CreateAlloca(ty, nullptr, "local var");

		auto var_ptr = local_var_to_llvm_addr_.at(wln->local());
        auto store = builder.CreateStore(val, var_ptr);
        return store;
	}
	default:
		throw std::runtime_error("lower_node: unsupported node kind " + std::to_string((int)a->kind()));
	}
}

std::vector<Value*> llvm_static_output_engine_impl::load_args(IRBuilder<> *builder, Argument *state_arg) {

	std::vector<Value *> ret = {
		state_arg, builder->CreateLoad(types.i64, reg_to_alloca_.at(reg_offsets::RDI)), builder->CreateLoad(types.i64, reg_to_alloca_.at(reg_offsets::RSI)),
		builder->CreateLoad(types.i64, reg_to_alloca_.at(reg_offsets::RDX)), builder->CreateLoad(types.i64, reg_to_alloca_.at(reg_offsets::RCX)),
		builder->CreateLoad(types.i64, reg_to_alloca_.at(reg_offsets::R8)), builder->CreateLoad(types.i64, reg_to_alloca_.at(reg_offsets::R9)),
//			  builder->CreateLoad(types.i512, reg_to_alloca_.at(reg_offsets::ZMM0)), builder->CreateLoad(types.i512, reg_to_alloca_.at(reg_offsets::ZMM1)),
//			  builder->CreateLoad(types.i512, reg_to_alloca_.at(reg_offsets::ZMM2)), builder->CreateLoad(types.i512, reg_to_alloca_.at(reg_offsets::ZMM3)),
//			  builder->CreateLoad(types.i512, reg_to_alloca_.at(reg_offsets::ZMM4)), builder->CreateLoad(types.i512, reg_to_alloca_.at(reg_offsets::ZMM5)),
//			  builder->CreateLoad(types.i512, reg_to_alloca_.at(reg_offsets::ZMM6)), builder->CreateLoad(types.i512, reg_to_alloca_.at(reg_offsets::ZMM7))
	};
	return ret;
};

void llvm_static_output_engine_impl::unwrap_ret(IRBuilder<> *builder, Value *value, Argument *state_arg) {
	builder->CreateStore(builder->CreateExtractValue(value, { 0 }), reg_to_alloca_.at(reg_offsets::RAX));
	builder->CreateStore(builder->CreateExtractValue(value, { 1 }), reg_to_alloca_.at(reg_offsets::RDX));
	//	builder->CreateStore(builder->CreateExtractValue(value, { 2 }), reg_to_alloca_.at(reg_offsets::ZMM0));
	//	builder->CreateStore(builder->CreateExtractValue(value, { 3 }), reg_to_alloca_.at(reg_offsets::ZMM1));
};

std::vector<Value*> llvm_static_output_engine_impl::wrap_ret(IRBuilder<> *builder, Argument *state_arg) {
	std::vector<Value*> ret;
	ret.push_back(builder->CreateLoad(types.i64, reg_to_alloca_.at(reg_offsets::RAX)));
	ret.push_back(builder->CreateLoad(types.i64, reg_to_alloca_.at(reg_offsets::RDX)));
	//	ret.push_back(builder->CreateLoad(types.i512, reg_to_alloca_.at(reg_offsets::ZMM0)));
	//	ret.push_back(builder->CreateLoad(types.i512, reg_to_alloca_.at(reg_offsets::ZMM1)));
	return ret;
}

void llvm_static_output_engine_impl::lower_chunk(IRBuilder<> *builder, Function *main_loop, std::shared_ptr<chunk> c)
{
	std::map<unsigned long, BasicBlock *> blocks;

	off_t addr = c->packets()[0]->address();
	Function *fn;
	if (addr != 0) {
		fn = fns_->at(addr);
		auto it = wrapper_fns_->find(c->name() + "_wrapper");
		if (it != wrapper_fns_->end()) {
			// Don't translate if we have a wrapper
			auto old = it->second;
			fn->replaceAllUsesWith(old);
			fn->eraseFromParent();
			GlobalAlias::create(get_fn_type(), old->getAddressSpace(), GlobalValue::LinkageTypes::ExternalLinkage, c->name(), old, module_.get());
			(*fns_)[addr] = old;
			return;
		}
		if (fn->begin() != fn->end())
			return;

		{
			Constant *gvar = module_->getOrInsertGlobal("guest_base", types.i8);
			gvar = reinterpret_cast<Constant *>(builder->CreateGEP(types.i8, gvar, ConstantInt::get(types.i64, addr)));
			func_map_.push_back(gvar);
			func_map_.push_back(fn);
		}
	} else {
		// Wrapper
		fn = wrapper_fns_->at(c->name());

		{
			// __arancini__ prefix has length 12, _wrapper suffix has length 8
			Constant *gvar = module_->getOrInsertGlobal("__guest__" + c->name().substr(12, c->name().size() - (8 + 12)), types.i8);
			func_map_.push_back(gvar);
			func_map_.push_back(fn);
		}
	}

#if defined(DEBUG)
	std::stringstream entry;
	entry << "do-static-" << fn->getName().str();
	std::stringstream exit;
	exit << "done-static-" << fn->getName().str();
#endif
	auto state_arg = fn->getArg(0);

	auto clk_ = module_->getOrInsertFunction("clk", types.clk_fn);

	auto pre = BasicBlock::Create(*llvm_context_, fn->getName()+"-pre", fn);
	auto mid = BasicBlock::Create(*llvm_context_, fn->getName()+"-mid");
	//auto pst = BasicBlock::Create(*llvm_context_, fn->getName()+"-pst", fn);
	auto dyn = BasicBlock::Create(*llvm_context_, fn->getName()+"-dyn");

	builder->SetInsertPoint(pre);
#if defined(DEBUG)
	builder->CreateCall(clk_, {state_arg, builder->CreateGlobalStringPtr(entry.str())});
#endif
	init_regs(*builder);
	restore_callee_regs(*builder, state_arg, false);

	builder->CreateStore(fn->getArg(1), reg_to_alloca_.at(reg_offsets::RDI));
	builder->CreateStore(fn->getArg(2), reg_to_alloca_.at(reg_offsets::RSI));
	builder->CreateStore(fn->getArg(3), reg_to_alloca_.at(reg_offsets::RDX));
	builder->CreateStore(fn->getArg(4), reg_to_alloca_.at(reg_offsets::RCX));
	builder->CreateStore(fn->getArg(5), reg_to_alloca_.at(reg_offsets::R8));
	builder->CreateStore(fn->getArg(6), reg_to_alloca_.at(reg_offsets::R9));

	//	builder->CreateStore(fn->getArg(7), reg_to_alloca_.at(reg_offsets::ZMM0));
	//	builder->CreateStore(fn->getArg(8), reg_to_alloca_.at(reg_offsets::ZMM1));
	//	builder->CreateStore(fn->getArg(9), reg_to_alloca_.at(reg_offsets::ZMM2));
	//	builder->CreateStore(fn->getArg(10), reg_to_alloca_.at(reg_offsets::ZMM3));
	//	builder->CreateStore(fn->getArg(11), reg_to_alloca_.at(reg_offsets::ZMM4));
	//	builder->CreateStore(fn->getArg(12), reg_to_alloca_.at(reg_offsets::ZMM5));
	//	builder->CreateStore(fn->getArg(13), reg_to_alloca_.at(reg_offsets::ZMM6));
	//	builder->CreateStore(fn->getArg(14), reg_to_alloca_.at(reg_offsets::ZMM7));
	auto pc_ptr = reg_to_alloca_.at(reg_offsets::PC);

	for (auto p : c->packets()) {
		std::stringstream block_name;
		block_name << "INSN_" << std::hex << p->address();
		auto b = BasicBlock::Create(*llvm_context_, block_name.str(), fn);
		blocks[p->address()] = b;
	}

	BasicBlock *packet_block = pre;
	for (auto p : c->packets()) {
		auto next_block = blocks[p->address()];

		if (packet_block != nullptr) {
			//falltrhough!
			auto ft = builder->CreateBr(next_block);
		}

		packet_block = next_block;

		builder->SetInsertPoint(packet_block);

		if (!p->actions().empty()) {
			for (const auto& a : p->actions()) {
				lower_node(*builder, state_arg, p, a.get());
			}
		}

		switch (p->updates_pc()) {
			case br_type::none:
			case br_type::sys:
				break;
			case br_type::call: {
				auto f = get_static_fn(p);
				if (f) {
					save_callee_regs(*builder, state_arg, false);
					auto ret = builder->CreateCall(f, load_args(builder, state_arg));
					restore_callee_regs(*builder, state_arg, false);
					unwrap_ret(builder, ret, state_arg);
				} else {
					save_callee_regs(*builder, state_arg);
					builder->CreateCall(main_loop, { state_arg });
					restore_callee_regs(*builder, state_arg);
				}
				break;
			}
			case br_type::br: {
				auto condbr = create_static_br(builder, p, &blocks, mid);
				if (!condbr) {
					builder->CreateBr(mid);
				}
				packet_block = nullptr;
				break;
			}
			case br_type::csel: {
				auto condbr = create_static_condbr(builder, p, &blocks, mid);
				if (!condbr) {
					builder->CreateBr(mid);
				}
				packet_block = nullptr;
				break;
			}
			case br_type::ret: {
				save_callee_regs(*builder, state_arg, false);
#if defined(DEBUG)
				builder->CreateCall(clk_, {state_arg, builder->CreateGlobalStringPtr(exit.str())});
#endif
			builder->CreateAggregateRet(wrap_ret(builder, state_arg).data(), 2);
			packet_block = nullptr;
			break;
		}
		}
	}

	if (packet_block != nullptr) {
		save_callee_regs(*builder, state_arg, false);
#if defined(DEBUG)
		builder->CreateCall(clk_, {state_arg, builder->CreateGlobalStringPtr(exit.str())});
#endif
		builder->CreateAggregateRet(wrap_ret(builder, state_arg).data(), 2);
	}

	mid->insertInto(fn);
	dyn->insertInto(fn);

	builder->SetInsertPoint(mid);
	Value* pc = builder->CreateLoad(types.i64, pc_ptr, "local-pc");
	if (!e_.is_exec()) {
		Value *gvar = module_->getOrInsertGlobal("guest_base", types.i8);
		gvar = builder->CreateGEP(types.i8, gvar, ConstantInt::get(types.i64, 0));
		gvar = builder->CreatePtrToInt(gvar, types.i64);
		pc = builder->CreateSub(pc, gvar);
	}
	auto fnswitch = builder->CreateSwitch(pc, dyn);

	for (auto p : blocks) {
		fnswitch->addCase(ConstantInt::get(types.i64,p.first), p.second);
	}

	builder->SetInsertPoint(dyn);
	save_callee_regs(*builder, state_arg);
	builder->CreateCall(main_loop, { state_arg });
	restore_callee_regs(*builder, state_arg);
#if defined(DEBUG)
	builder->CreateCall(clk_, {state_arg, builder->CreateGlobalStringPtr(exit.str())});
#endif
	builder->CreateAggregateRet(wrap_ret(builder, state_arg).data(), 2);

	if (verifyFunction(*fn, &errs())) {
		module_->print(errs(), nullptr);
		throw std::runtime_error("function verification failed");
	}
}

FunctionType *llvm_static_output_engine_impl::get_fn_type() {
	std::vector<Type*> argv;
	StructType* retv;

	retv = StructType::get(*llvm_context_, { types.i64, types.i64 /*, types.i512, types.i512*/ }, false);
	argv = {
		types.cpu_state_ptr, types.i64, types.i64, types.i64, types.i64, types.i64, types.i64,
		//		types.i512,
		//		types.i512,
		//		types.i512,
		//		types.i512,
		//		types.i512,
		//		types.i512,
		//		types.i512,
		//		types.i512,
	}; // cpu_state, 6 int args, 8 float args
	return FunctionType::get(retv, argv, false);
}

void llvm_static_output_engine_impl::init_regs(IRBuilder<> &builder) {
	reg_to_alloca_.clear();
#define DEFREG(ctype, ltype, name) do { \
	reg_to_alloca_[reg_offsets::name] = builder.CreateAlloca(types.ltype, 128, nullptr, "reg"#name); \
	builder.CreateStore(ConstantInt::get(types.ltype, 0), reg_to_alloca_.at(reg_offsets::name)); \
} while (0);
#include <arancini/input/x86/reg.def>
#undef DEFREG
}

void llvm_static_output_engine_impl::save_all_regs(IRBuilder<> &builder, Argument *state_arg) {
	auto regs = {
#define DEFREG(ctype, ltype, name) reg_offsets::name,
#include <arancini/input/x86/reg.def>
#undef DEFREG
	};
	for (auto reg : regs) {
		auto ptr = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, off_to_idx.at((unsigned long)reg)) }, "save_"+std::to_string((unsigned long)reg));
		auto alloca = reg_to_alloca_.at(reg);
		StoreInst *store = builder.CreateStore(builder.CreateLoad(alloca->getAllocatedType(), alloca), ptr);
		store->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(store->getContext(), reg_file_alias_scope_));
		store->setMetadata(LLVMContext::MD_noalias, MDNode::get(store->getContext(), guest_mem_alias_scope_));
	}
}

void llvm_static_output_engine_impl::restore_all_regs(IRBuilder<> &builder, Argument *state_arg) {
	auto regs = {
#define DEFREG(ctype, ltype, name) reg_offsets::name,
#include <arancini/input/x86/reg.def>
#undef DEFREG
	};
	for (auto reg : regs) {
		auto ptr = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, off_to_idx.at((unsigned long)reg)) }, "restore_"+std::to_string((unsigned long)reg));
		auto alloca = reg_to_alloca_.at(reg);
		LoadInst *load = builder.CreateLoad(alloca->getAllocatedType(), ptr);
		load->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(load->getContext(), reg_file_alias_scope_));
		load->setMetadata(LLVMContext::MD_noalias, MDNode::get(load->getContext(), guest_mem_alias_scope_));
		builder.CreateStore(load, alloca);
	}
}

// well akshually, callee saved registers and permanent state
void llvm_static_output_engine_impl::save_callee_regs(IRBuilder<> &builder, Argument *state_arg, bool with_args)
{
	auto args = {
		reg_offsets::RCX, reg_offsets::RDX, reg_offsets::RDI, reg_offsets::RSI, reg_offsets::R8, reg_offsets::R9
	};
	auto regs = { reg_offsets::PC, reg_offsets::RBX, reg_offsets::RSP, reg_offsets::RBP, reg_offsets::R12, reg_offsets::R13, reg_offsets::R14, reg_offsets::R15,
		reg_offsets::FS, reg_offsets::GS , reg_offsets::X87_STS, reg_offsets::X87_TAG, reg_offsets::X87_CTRL, reg_offsets::ZMM0,
reg_offsets::ZMM1, reg_offsets::ZMM2, reg_offsets::ZMM3, reg_offsets::ZMM4, reg_offsets::ZMM5, reg_offsets::ZMM6, reg_offsets::ZMM7 };
	for (auto reg : regs) {
		auto ptr = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, off_to_idx.at((unsigned long)reg)) }, "save_"+std::to_string((unsigned long)reg));
		auto alloca = reg_to_alloca_.at(reg);
		StoreInst *store = builder.CreateStore(builder.CreateLoad(alloca->getAllocatedType(), alloca), ptr);
		store->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(store->getContext(), reg_file_alias_scope_));
		store->setMetadata(LLVMContext::MD_noalias, MDNode::get(store->getContext(), guest_mem_alias_scope_));
	}
	if (!with_args) return;
	for (auto reg : args) {
		auto ptr = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, off_to_idx.at((unsigned long)reg)) }, "save_"+std::to_string((unsigned long)reg));
		auto alloca = reg_to_alloca_.at(reg);
		StoreInst *store = builder.CreateStore(builder.CreateLoad(alloca->getAllocatedType(), alloca), ptr);
		store->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(store->getContext(), reg_file_alias_scope_));
		store->setMetadata(LLVMContext::MD_noalias, MDNode::get(store->getContext(), guest_mem_alias_scope_));
	}
}

void llvm_static_output_engine_impl::restore_callee_regs(IRBuilder<> &builder, Argument *state_arg, bool with_rets)
{
	auto rets = { reg_offsets::RAX, reg_offsets::RDX };
	auto regs = { reg_offsets::PC, reg_offsets::RBX, reg_offsets::RSP, reg_offsets::RBP, reg_offsets::R12, reg_offsets::R13, reg_offsets::R14, reg_offsets::R15,
		reg_offsets::FS, reg_offsets::GS , reg_offsets::X87_STACK_BASE, reg_offsets::X87_STS, reg_offsets::X87_TAG, reg_offsets::X87_CTRL, reg_offsets::ZMM0, reg_offsets::ZMM1 };
	for (auto reg : regs) {
		auto ptr = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, off_to_idx.at((unsigned long)reg)) }, "restore_"+std::to_string((unsigned long)reg));
		auto alloca = reg_to_alloca_.at(reg);
		LoadInst *load = builder.CreateLoad(alloca->getAllocatedType(), ptr);
		load->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(load->getContext(), reg_file_alias_scope_));
		load->setMetadata(LLVMContext::MD_noalias, MDNode::get(load->getContext(), guest_mem_alias_scope_));
		builder.CreateStore(load, alloca);
	}
	if (!with_rets) return;
	for (auto reg : rets) {
		auto ptr = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, off_to_idx.at((unsigned long)reg)) }, "restore_"+std::to_string((unsigned long)reg));
		auto alloca = reg_to_alloca_.at(reg);
		LoadInst *load = builder.CreateLoad(alloca->getAllocatedType(), ptr);
		load->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(load->getContext(), reg_file_alias_scope_));
		load->setMetadata(LLVMContext::MD_noalias, MDNode::get(load->getContext(), guest_mem_alias_scope_));
		builder.CreateStore(load, alloca);
	}
}

Function *llvm_static_output_engine_impl::get_static_fn(std::shared_ptr<packet> pkt) {

	std::shared_ptr<action_node> node = nullptr;
	for (auto a : pkt->actions()) {
		if(a->kind() == node_kinds::write_pc && a->updates_pc()==br_type::call)
			node = a;
	}
	if (node != nullptr) {
		auto wpn = std::static_pointer_cast<write_pc_node>(node);
		if (wpn->const_target()) {

			auto it = fns_->find(wpn->const_target()+pkt->address());
			auto ret = it != fns_->end() ? it->second : nullptr;
			if (ret)
				fixed_branches++;
			return ret;
		}
	}
	return nullptr;
};

void llvm_static_output_engine_impl::optimise()
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

	PB.registerOptimizerLastEPCallback( [&](ModulePassManager &mpm, OptimizationLevel Level) {
		mpm.addPass(createModuleToFunctionPassAdaptor(FenceCombinePass()));
	});

	ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O2);
	MPM.run(*module_, MAM);

	// Compiled modules now exists
	if (e_.dbg_) {
		std::cerr << "Fixed branches: " << fixed_branches << std::endl;
	}
	if (e_.debug_dump_filename.has_value()) {
		std::error_code EC;
		std::string filename = e_.debug_dump_filename.value() + ".opt.ll";
		raw_fd_ostream file(filename, EC);

		if (EC) {
			errs() << "Error opening file '" << filename << "': " << EC.message() << "\n";
		}

		module_->print(file, nullptr);
		file.close();
	}
}

void llvm_static_output_engine_impl::compile()
{
	std::cout << "Fixed branches: " << fixed_branches << std::endl;
#ifndef CROSS_TRANSLATE
	auto TT = sys::getDefaultTargetTriple();
#elif defined(ARCH_RISCV64)
	auto TT = "riscv64-unknown-linux-gnu";
#elif defined(ARCH_AARCH64)
	auto TT = "aarch64-unknown-linux-gnu";
#elif defined(ARCH_X86_64)
	auto TT = "x86_64-unknown-linux-gnu";
#error Please check if the triple above is correct
#endif
	module_->setTargetTriple(TT);

	std::string error_message;
	auto T = TargetRegistry::lookupTarget(TT, error_message);
	if (!T) {
		throw std::runtime_error(error_message);
	}

	TargetOptions TO;
	auto RM = optional<Reloc::Model>(Reloc::Model::PIC_);
#if defined(ARCH_RISCV64)
	//Add multiply(M), atomics(A), single(F) and double(D) precision float and compressed(C) extensions
	const char *features = "+m,+a,+f,+d,+c,-v,+fast-unaligned-access,+xtheadba,+xtheadbb,+xtheadbs,+xtheadcmo,+xtheadcondmov,+xtheadfmemidx,+xtheadmac,+xtheadmemidx,+xtheadmempair,+xtheadsync";
	const char *cpu = "generic-rv64";
	//Specify abi as 64 bits using double float registers
	TO.MCOptions.ABIName="lp64d";
#elif defined(ARCH_AARCH64)
	const char *features = "+fp-armv8,+v8.5a,+lse,+ls64";
	const char *cpu = "thunderx2t99";
#else
	const char *features = "+avx";
	const char *cpu = "generic";
#endif

	auto TM = T->createTargetMachine(TT, cpu, features, TO, RM);

	module_->setDataLayout(TM->createDataLayout());
	module_->setPICLevel(PICLevel::BigPIC);

	std::error_code EC;
	raw_fd_ostream output_file(e_.output_filename(), EC, sys::fs::OF_None);

	if (EC) {
		throw std::runtime_error("could not create output file: " + EC.message());
	}

	legacy::PassManager OPM;
	if (TM->addPassesToEmitFile(OPM, output_file, nullptr, ::llvm::CodeGenFileType::ObjectFile)) {
		throw std::runtime_error("unable to emit file");
	}

	OPM.run(*module_);
}
