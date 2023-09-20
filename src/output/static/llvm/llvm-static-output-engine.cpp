#include "arancini/ir/node.h"
#include "arancini/ir/port.h"
#include "arancini/ir/visitor.h"
#include "llvm/Support/raw_ostream.h"
#include <arancini/ir/chunk.h>
#include <arancini/output/static/llvm/llvm-static-output-engine-impl.h>
#include <arancini/output/static/llvm/llvm-static-output-engine.h>
#include <cstdint>
#include <iostream>
#include <llvm/ADT/FloatingPointMode.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/Support/TimeProfiler.h>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>

using namespace arancini::output::o_static::llvm;
using namespace arancini::ir;
using namespace ::llvm;

llvm_static_output_engine::llvm_static_output_engine(const std::string &output_filename)
	: static_output_engine(output_filename)
	, oei_(std::make_unique<llvm_static_output_engine_impl>(*this, chunks()))
	, dbg_(false)
{
}

llvm_static_output_engine::~llvm_static_output_engine() = default;

void llvm_static_output_engine::generate() { oei_->generate(); }

llvm_static_output_engine_impl::llvm_static_output_engine_impl(const llvm_static_output_engine &e, const std::vector<std::shared_ptr<ir::chunk>> &chunks)
	: e_(e)
	, chunks_(chunks)
	, llvm_context_(std::make_unique<LLVMContext>())
	, module_(std::make_unique<Module>("generated", *llvm_context_))
	, in_br(false)
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
	types.main_fn = FunctionType::get(types.i32, { types.i32, PointerType::get(Type::getInt8PtrTy(*llvm_context_), 0) }, false);
	types.loop_fn = FunctionType::get(types.vd, { types.cpu_state_ptr }, false);
	types.init_dbt = FunctionType::get(types.cpu_state_ptr, { types.i64, types.i32, PointerType::get(Type::getInt8PtrTy(*llvm_context_),0) }, false);
	types.dbt_invoke = FunctionType::get(types.i32, { types.cpu_state_ptr }, false);
	types.internal_call_handler = FunctionType::get(types.i32, { types.cpu_state_ptr, types.i32 }, false);
	types.finalize = FunctionType::get(types.vd, {}, false);
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
	builder.CreateCall(loop_fn, { init_dbt_result });
	builder.CreateRet(ConstantInt::get(types.i32, 0));

	builder.SetInsertPoint(fail_block);
	builder.CreateRet(ConstantInt::get(types.i32, 1));
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

	auto loop_fn = Function::Create(types.loop_fn, GlobalValue::LinkageTypes::ExternalLinkage, "MainLoop", *module_);
	loop_fn->addParamAttr(0, Attribute::AttrKind::NoCapture);
	loop_fn->addParamAttr(0, Attribute::AttrKind::NoAlias);
	loop_fn->addParamAttr(0, Attribute::AttrKind::NoUndef);

	create_main_function(loop_fn);

	// TODO: Input Arch Specific (maybe need some kind of descriptor?)

	auto state_arg = loop_fn->getArg(0);

	auto entry_block = BasicBlock::Create(*llvm_context_, "entry", loop_fn);
	auto loop_block = BasicBlock::Create(*llvm_context_, "loop", loop_fn);
	auto switch_to_dbt = BasicBlock::Create(*llvm_context_, "switch_to_dbt", loop_fn);
	auto check_for_int_call_block = BasicBlock::Create(*llvm_context_, "check_for_internal_call", loop_fn);
	auto internal_call_block = BasicBlock::Create(*llvm_context_, "internal_call", loop_fn);
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
	builder.CreateCondBr(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, invoke_result, ConstantInt::get(types.i32, 0)), loop_block, check_for_int_call_block);

	builder.SetInsertPoint(check_for_int_call_block);
	builder.CreateCondBr(builder.CreateCmp(CmpInst::Predicate::ICMP_SGT, invoke_result, ConstantInt::get(types.i32, 0)), internal_call_block, exit_block);

	builder.SetInsertPoint(internal_call_block);
	auto internal_call_callee = module_->getOrInsertFunction("execute_internal_call", types.internal_call_handler);
	auto internal_call_result = builder.CreateCall(internal_call_callee, { state_arg, invoke_result });
	builder.CreateCondBr(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, internal_call_result, ConstantInt::get(types.i32, 0)), loop_block, exit_block);


	builder.SetInsertPoint(exit_block);

	auto finalize_call_callee = module_->getOrInsertFunction("finalize", types.finalize);
	builder.CreateCall(finalize_call_callee, {});

	builder.CreateRetVoid();

	if (e_.dbg_) {
		module_->print(outs(), nullptr);
	}

	if (verifyFunction(*loop_fn, &errs())) {
		throw std::runtime_error("function verification failed");
	}
}

void llvm_static_output_engine_impl::lower_chunks(SwitchInst *pcswitch, BasicBlock *contblock)
{
	auto blocks = std::make_shared<std::map<unsigned long, BasicBlock *>>();
	for (auto c : chunks_) {
		lower_chunk(pcswitch, contblock, c, blocks);
	}
	for (auto b : *blocks) {
		pcswitch->addCase(ConstantInt::get(types.i64, b.first), b.second);
	}
}

static const char *regnames[] = {
#define DEFREG(ctype, ltype, name) "" #name,
#include <arancini/input/x86/reg.def>
#undef DEFREG
};

static std::string reg_name(int regidx)
{
	if ((size_t)regidx < (sizeof(regnames) / sizeof(regnames[0]))) {
		return regnames[regidx];
	}

	return "guestreg";
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

		auto address_ptr = builder.CreateIntToPtr(address, PointerType::get(ty, 256));

		if (auto address_ptr_i = ::llvm::dyn_cast<Instruction>(address_ptr)) {
			address_ptr_i->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(*llvm_context_, guest_mem_alias_scope_));
		}

		LoadInst *li = builder.CreateLoad(ty, address_ptr);
		li->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(li->getContext(), guest_mem_alias_scope_));
		return li;
	}

	case node_kinds::read_reg: {
		auto rrn = (read_reg_node *)n;
		auto src_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, rrn->regidx()) },
			reg_name(rrn->regidx()));

		if (auto src_reg_i = ::llvm::dyn_cast<Instruction>(src_reg)) {
			src_reg_i->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(*llvm_context_, reg_file_alias_scope_));
		}

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
				if (lhs->getType()->getIntegerBitWidth() > rhs->getType()->getIntegerBitWidth())
					rhs = builder.CreateSExt(rhs, lhs->getType());

				if (lhs->getType()->getIntegerBitWidth() < rhs->getType()->getIntegerBitWidth())
					lhs = builder.CreateSExt(lhs, rhs->getType());
			}

			if (lhs->getType()->isFloatingPointTy()) {
				switch (ban->op()) {
					case binary_arith_op::bxor:
					case binary_arith_op::band:
					case binary_arith_op::bor: {
						lhs = builder.CreateBitCast(lhs, IntegerType::get(*llvm_context_, lhs->getType()->getPrimitiveSizeInBits())); break;
					default: break;
					}
				}
			}
			if (rhs->getType()->isFloatingPointTy()) {
				switch (ban->op()) {
					case binary_arith_op::bxor:
					case binary_arith_op::band:
					case binary_arith_op::bor: {
						rhs = builder.CreateBitCast(lhs, IntegerType::get(*llvm_context_, rhs->getType()->getPrimitiveSizeInBits())); break;
					default: break;
					}
				}
			}

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
				if (lhs->getType()->isFloatingPointTy())
					return builder.CreateFSub(lhs, rhs);
				return builder.CreateSub(lhs, rhs);
			}
			case binary_arith_op::mul: {
				if (lhs->getType()->isFloatingPointTy())
					return builder.CreateFMul(lhs, rhs);
				return builder.CreateMul(lhs, rhs);
			}
			case binary_arith_op::div: {
				if (lhs->getType()->isFloatingPointTy())
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

		return ConstantInt::get(types.i64, pkt->address());
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
			if (val->getType()->isVectorTy()) {
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
					throw std::runtime_error("unsupported bitcast element width");
				}
				return builder.CreateBitCast(val, ty);

			}
			return val;
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
		if (v->getType()->isFloatingPointTy())
			v = builder.CreateBitCast(v, IntegerType::getIntNTy(*llvm_context_, v->getType()->getPrimitiveSizeInBits()));

		switch (p.kind()) {
		case port_kinds::value: {

			switch (un->op()) {
			case unary_arith_op::bnot:
				return builder.CreateNot(v);
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
			amount = builder.CreateZExt(amount, input->getType());

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
		auto lhs = lower_port(builder, state_arg, pkt, ban->address());
		lhs = builder.CreateLoad(rhs->getType(), builder.CreateIntToPtr(lhs, PointerType::get(rhs->getType(), 256)), "Atomic LHS");
		auto value_port = lower_port(builder, state_arg, pkt, n->val());
		
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
		if (p.kind() == port_kinds::value)
			return lower_node(builder, state_arg, pkt, n);
		// TODO: Flags
		return ConstantInt::get(types.i8, 0);
	}
	case node_kinds::read_local: {
	        auto rln = (read_local_node *)n;
	        auto address = local_var_to_llvm_addr_.at(rln->local());
	        ::llvm::Type *ty;
	        switch (rln->local()->type().width()) {
	                case 80: ty = types.f80; break;
	                default: throw std::runtime_error("unsupported read_local width"+std::to_string(rln->local()->type().width()));
	        }
	        auto load = builder.CreateLoad(ty, address, "read_local");
	        load->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(load->getContext(), guest_mem_alias_scope_));
	        return load;
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

		auto dest_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, wrn->regidx()) },
			reg_name(wrn->regidx()));

		auto *reg_type = ((GetElementPtrInst*)dest_reg)->getResultElementType();

		if (auto dest_reg_i = ::llvm::dyn_cast<Instruction>(dest_reg)) {
			dest_reg_i->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(*llvm_context_, reg_file_alias_scope_));
		}

		auto val = lower_port(builder, state_arg, pkt, wrn->value());

		//Bitcast the resulting value to the type of the register
		//since the operations can happen on different type after
		//other casting operations
		if(val->getType()->isVectorTy())
			val = builder.CreateBitCast(val, IntegerType::get(*llvm_context_, val->getType()->getPrimitiveSizeInBits()));
		auto reg_val = builder.CreateZExtOrBitCast(val, reg_type);

		return builder.CreateStore(reg_val, dest_reg);
	}

	case node_kinds::write_mem: {
		auto wmn = (write_mem_node *)a;

		auto address = lower_port(builder, state_arg, pkt, wmn->address());
		auto value = lower_port(builder, state_arg, pkt, wmn->value());

		auto address_ptr = builder.CreateIntToPtr(address, PointerType::get(value->getType(), 256));

		if (auto address_ptr_i = ::llvm::dyn_cast<Instruction>(address_ptr)) {
			address_ptr_i->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(*llvm_context_, guest_mem_alias_scope_));
		}

		auto store = builder.CreateStore(value, address_ptr);
		store->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(store->getContext(), guest_mem_alias_scope_));
		return store;
	}

	case node_kinds::write_pc: {
		auto wpn = (write_pc_node *)a;

		auto dest_reg = builder.CreateGEP(types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, 0) }, "pcptr");

		auto val = lower_port(builder, state_arg, pkt, wpn->value());

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
		auto rhs = lower_port(builder, state_arg, pkt, ban->rhs());

		auto existing = node_ports_to_llvm_values_.find(&ban->val());
		if (existing != node_ports_to_llvm_values_.end())
			return existing->second;

		lhs = builder.CreateIntToPtr(lhs, PointerType::get(rhs->getType(), 256));
		AtomicRMWInst *out = nullptr;
		switch(ban->op()) {
			case binary_atomic_op::add: builder.CreateAtomicRMW(AtomicRMWInst::Add, lhs, rhs, Align(8), AtomicOrdering::SequentiallyConsistent); break;
			case binary_atomic_op::sub: builder.CreateAtomicRMW(AtomicRMWInst::Sub, lhs, rhs, Align(8), AtomicOrdering::SequentiallyConsistent); break;
			case binary_atomic_op::xadd: out = builder.CreateAtomicRMW(AtomicRMWInst::Add, lhs, rhs, Align(8), AtomicOrdering::SequentiallyConsistent); break;
			case binary_atomic_op::bor: out = builder.CreateAtomicRMW(AtomicRMWInst::Or, lhs, rhs, Align(8), AtomicOrdering::SequentiallyConsistent); break;
			case binary_atomic_op::xchg: out = builder.CreateAtomicRMW(AtomicRMWInst::Xchg, lhs, rhs, Align(8), AtomicOrdering::SequentiallyConsistent); break;
			default: throw std::runtime_error("unsupported bin atomic operation " + std::to_string((int)ban->op()));
		}
		if (out) {
			switch (ban->op()) {
				//case binary_atomic_op::xadd: out = builder.CreateAtomicRMW(AtomicRMWInst::Xchg, lhs, out, Align(64), AtomicOrdering::SequentiallyConsistent); break;
				default: break;
			}
			node_ports_to_llvm_values_[&ban->val()] = out;
			return out;
		}
		auto ret = builder.CreateLoad(rhs->getType(), builder.CreateIntToPtr(lhs, PointerType::get(rhs->getType(), 256)), "Atomic result");
		node_ports_to_llvm_values_[&ban->val()] = ret;
		return ret;
	}
	case node_kinds::ternary_atomic: {
		auto tan = (ternary_atomic_node *)a;
		auto lhs = lower_port(builder, state_arg, pkt, tan->address());
		auto rhs = lower_port(builder, state_arg, pkt, tan->rhs());
		auto top = lower_port(builder, state_arg, pkt, tan->top());

		Value *out;
		switch(tan->op()) {
			case ternary_atomic_op::cmpxchg: {
				lhs = builder.CreateIntToPtr(lhs, PointerType::get(rhs->getType(), 256));
				auto instr = builder.CreateAtomicCmpXchg(lhs, rhs, top, Align(8), AtomicOrdering::SequentiallyConsistent, AtomicOrdering::SequentiallyConsistent);
				return instr;
			}
			default: throw std::runtime_error("unsupported tern atomic operation " + std::to_string((int)tan->op()));
		}
		return out;
	}
	case node_kinds::internal_call: {
        auto icn = (internal_call_node *)a;
        auto switch_callee = module_->getOrInsertFunction("execute_internal_call", types.internal_call_handler);
        if (icn->fn().name() == "handle_syscall") {
                return builder.CreateCall(switch_callee, { state_arg, ConstantInt::get(types.i32, 1) });
        }
        throw std::runtime_error("unsupported internal call type" + icn->fn().name());
	}
	case node_kinds::write_local: {
        auto wln = (write_local_node *)a;
        auto val = lower_port(builder, state_arg, pkt, wln->write_value());
        ::llvm::Type *ty;
        switch (wln->write_value().type().width()) {
                case 80: ty = types.f80; break;
                default: throw std::runtime_error("unsupported alloca width");
        }
        auto var_ptr = builder.CreateAlloca(ty, nullptr, "local var");
        local_var_to_llvm_addr_[wln->local()] = var_ptr;

        auto store = builder.CreateStore(val, var_ptr);
        store->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(store->getContext(), guest_mem_alias_scope_));
        return store;
	}
	default:
		throw std::runtime_error("lower_node: unsupported node kind " + std::to_string((int)a->kind()));
	}
}

void llvm_static_output_engine_impl::lower_chunk(SwitchInst *pcswitch, BasicBlock *contblock, std::shared_ptr<chunk> c, std::shared_ptr<std::map<unsigned long, BasicBlock *>> blocks)
{
	IRBuilder<> builder(*llvm_context_);
	//std::map<unsigned long, BasicBlock *> blocks;

	if (c->packets().empty()) {
		return;
	}

	auto state_arg = contblock->getParent()->getArg(0);

	for (auto p : c->packets()) {
		std::stringstream block_name;
		block_name << "INSN_" << std::hex << p->address();

		BasicBlock *block = BasicBlock::Create(*llvm_context_, block_name.str(), contblock->getParent());
		(*blocks)[p->address()] = block;
	}

	BasicBlock *packet_block = nullptr;
	for (auto p : c->packets()) {
		auto next_block = (*blocks)[p->address()];

		if (packet_block != nullptr) {
			builder.CreateBr(next_block);
		}

		packet_block = next_block;

		builder.SetInsertPoint(packet_block);

		if (!p->actions().empty()) {
			for (const auto& a : p->actions()) {
				lower_node(builder, state_arg, p, a.get());
			}
		}

		if (p->updates_pc()) {
			builder.CreateBr(contblock);
			packet_block = nullptr;
		}
	}

	if (packet_block != nullptr) {
		builder.CreateBr(contblock);
	}
}

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

	ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O2);

	MPM.run(*module_, MAM);

	// Compiled modules now exists
	if (e_.dbg_) {
		module_->print(outs(), nullptr);
	}
}

void llvm_static_output_engine_impl::compile()
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
#if defined(ARCH_RISCV64)
	//Add multiply(M), atomics(A), single(F) and double(D) precision float and compressed(C) extensions
	const char *features = "+m,+a,+f,+d,+c";
	const char *cpu = "generic-rv64";
	//Specify abi as 64 bits using double float registers
	TO.MCOptions.ABIName="lp64d";
#elif defined(ARCH_AARCH64)
	const char *features = "+fp-armv8,+v8.5a";
	const char *cpu = "neoverse-n2";
#else
	const char *features = "+avx";
	const char *cpu = "generic";
#endif

	auto TM = T->createTargetMachine(TT, cpu, features, TO, RM);

	module_->setDataLayout(TM->createDataLayout());

	std::error_code EC;
	raw_fd_ostream output_file(e_.output_filename(), EC, sys::fs::OF_None);

	if (EC) {
		throw std::runtime_error("could not create output file: " + EC.message());
	}

	legacy::PassManager OPM;
	if (TM->addPassesToEmitFile(OPM, output_file, nullptr, CGFT_ObjectFile)) {
		throw std::runtime_error("unable to emit file");
	}

	OPM.run(*module_);
}
