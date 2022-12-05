#include "llvm/Support/raw_ostream.h"
#include <arancini/ir/chunk.h>
#include <arancini/output/static/llvm/llvm-static-output-engine-impl.h>
#include <arancini/output/static/llvm/llvm-static-output-engine.h>
#include <iostream>
#include <map>
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
	types.i128 = Type::getInt128Ty(*llvm_context_);

	// CPU State

	// 0 PC, RAX, RCX, RDX, RBX, RSP, RBP, RSI, RDI, R8, R9, R10, R11, R12, R13, R14, R15,
	// 17 ZF, 18 CF, 19 OF, 20 SF, 21 PF
	// 22 XMM0...
	// 38 FS, 38 GS
	auto state_elements = std::vector<Type *>({
		types.i64, // 0: RIP
		types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, // 1: AX, CX, DX, BX, SP, BP, SI, DI
		types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, types.i64, // 9: R8, R9, R10, R11, R12, R13, R14, R15
		types.i8, types.i8, types.i8, types.i8, types.i8, // 17: ZF, CF, OF, SF, PF
		types.i128, types.i128, types.i128, types.i128, types.i128, types.i128, types.i128, types.i128, // 22: XMM0--7
		types.i128, types.i128, types.i128, types.i128, types.i128, types.i128, types.i128, types.i128, // 30: XMM8--15
		types.i64, types.i64 // 38: FS, GS
	});

	types.cpu_state = StructType::get(*llvm_context_, state_elements, false);
        if (!types.cpu_state->isLiteral())
	  types.cpu_state->setName("cpu_state_struct");
	types.cpu_state_ptr = PointerType::get(types.cpu_state, 0);

	// Functions
	types.main_fn = FunctionType::get(types.i32, { types.i32, PointerType::get(Type::getInt8PtrTy(*llvm_context_), 0) }, false);
	types.loop_fn = FunctionType::get(types.vd, { types.cpu_state_ptr }, false);
	types.init_dbt = FunctionType::get(types.cpu_state_ptr, { types.i64 }, false);
	types.dbt_invoke = FunctionType::get(types.i32, { types.cpu_state_ptr }, false);
}

void llvm_static_output_engine_impl::create_main_function(Function *loop_fn)
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

	if (e_.dbg_) {
		module_->print(outs(), nullptr);
	}

	if (verifyFunction(*loop_fn, &errs())) {
		throw std::runtime_error("function verification failed");
	}
}

void llvm_static_output_engine_impl::lower_chunks(SwitchInst *pcswitch, BasicBlock *contblock)
{
	for (auto c : chunks_) {
		lower_chunk(pcswitch, contblock, c);
	}
}

static const char *regnames[]
	= { "rip", "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "zf", "cf", "of", "sf", "pf" };

static std::string reg_name(int regoff)
{
	if ((size_t)regoff < (sizeof(regnames) / sizeof(regnames[0]))) {
		return regnames[regoff];
	}

	return "guestreg";
}

Value *llvm_static_output_engine_impl::materialise_port(IRBuilder<> &builder, Argument *state_arg, std::shared_ptr<packet> pkt, port &p)
{
	auto n = p.owner();

	switch (p.owner()->kind()) {
	case node_kinds::constant: {
		auto cn = (constant_node *)n;

		switch (cn->val().type().width()) {
		case 1:
		case 8:
			return ConstantInt::get(types.i8, cn->const_val_i());
		case 16:
			return ConstantInt::get(types.i16, cn->const_val_i());
		case 32:
			return ConstantInt::get(types.i32, cn->const_val_i());
		case 64:
			return ConstantInt::get(types.i64, cn->const_val_i());

		default:
			throw std::runtime_error("unsupported constant width");
		}
	}

	case node_kinds::read_mem: {
		auto rmn = (read_mem_node *)n;
		auto address = lower_port(builder, state_arg, pkt, rmn->address());

		auto address_ptr = builder.CreateIntToPtr(address, PointerType::get(types.i64, 256));

		if (auto address_ptr_i = ::llvm::dyn_cast<Instruction>(address_ptr)) {
			address_ptr_i->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(*llvm_context_, guest_mem_alias_scope_));
		}

		LoadInst *li;
		switch (rmn->val().type().width()) {
		case 8:
			li = builder.CreateLoad(types.i8, address_ptr);
			break;
		case 16:
			li = builder.CreateLoad(types.i16, address_ptr);
			break;
		case 32:
			li = builder.CreateLoad(types.i32, address_ptr);
			break;
		case 64:
			li = builder.CreateLoad(types.i64, address_ptr);
			break;
		case 128:
			li = builder.CreateLoad(types.i128, address_ptr);
			break;

		default:
			throw std::runtime_error("unsupported memory load width");
		}

		li->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(li->getContext(), guest_mem_alias_scope_));
		return li;
	}

	case node_kinds::read_reg: {
		auto rrn = (read_reg_node *)n;
		auto src_reg = builder.CreateGEP(
			types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, rrn->regoff()) }, reg_name(rrn->regoff()));

		if (auto src_reg_i = ::llvm::dyn_cast<Instruction>(src_reg)) {
			src_reg_i->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(*llvm_context_, reg_file_alias_scope_));
		}

		switch (rrn->val().type().width()) {
		case 1:
			return builder.CreateLoad(types.i1, src_reg);
		case 8:
			return builder.CreateLoad(types.i8, src_reg);
		case 16:
			return builder.CreateLoad(types.i16, src_reg);
		case 32:
			return builder.CreateLoad(types.i32, src_reg);
		case 64:
			return builder.CreateLoad(types.i64, src_reg);
		case 128:
			return builder.CreateLoad(types.i128, src_reg);

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
			case binary_arith_op::bor:
				return builder.CreateOr(lhs, rhs);
			case binary_arith_op::add:
				return builder.CreateAdd(lhs, rhs);
			case binary_arith_op::sub:
				return builder.CreateSub(lhs, rhs);
			case binary_arith_op::mul:
				return builder.CreateMul(lhs, rhs);
			case binary_arith_op::div:
				return builder.CreateUDiv(lhs, rhs);
			case binary_arith_op::cmpeq:
				return builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, lhs, rhs);
			case binary_arith_op::cmpne:
				return builder.CreateCmp(CmpInst::Predicate::ICMP_NE, lhs, rhs);

			default:
				throw std::runtime_error("unsupported binary operator " + std::to_string((int)ban->op()));
			}
		} else if (p.kind() == port_kinds::zero) {
			auto value_port = lower_port(builder, state_arg, pkt, n->val());
			return builder.CreateZExt(builder.CreateCmp(CmpInst::Predicate::ICMP_EQ, value_port, ConstantInt::get(value_port->getType(), 0)), types.i8);
		} else if (p.kind() == port_kinds::negative) {
			auto value_port = lower_port(builder, state_arg, pkt, n->val());
			return builder.CreateZExt(builder.CreateCmp(CmpInst::Predicate::ICMP_SLT, value_port, ConstantInt::get(value_port->getType(), 0)), types.i8);
		} else {
			// TODO: Need to support CARRY + OVERFLOW
			return ConstantInt::get(types.i8, 0);
			// throw std::runtime_error("unsupported port kind");
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

		switch (cn->op()) {
		case cast_op::zx:
			switch (cn->val().type().width()) {
			case 8:
				return builder.CreateZExt(val, types.i8);
			case 16:
				return builder.CreateZExt(val, types.i16);
			case 32:
				return builder.CreateZExt(val, types.i32);
			case 64:
				return builder.CreateZExt(val, types.i64);
			case 128:
				return builder.CreateZExt(val, types.i128);

			default:
				throw std::runtime_error("unsupported zx width " + std::to_string(cn->val().type().width()));
			}

		case cast_op::sx:
			switch (cn->val().type().width()) {
			case 16:
				return builder.CreateSExt(val, types.i16);
			case 32:
				return builder.CreateSExt(val, types.i32);
			case 64:
				return builder.CreateSExt(val, types.i64);
			case 128:
				return builder.CreateSExt(val, types.i128);

			default:
				throw std::runtime_error("unsupported sx width");
			}

		case cast_op::trunc:
			switch (cn->val().type().width()) {
			case 1:
				return builder.CreateTrunc(val, types.i1);
			case 8:
				return builder.CreateTrunc(val, types.i8);
			case 16:
				return builder.CreateTrunc(val, types.i16);
			case 32:
				return builder.CreateTrunc(val, types.i32);
			case 64:
				return builder.CreateTrunc(val, types.i64);

			default:
				throw std::runtime_error("unsupported trunc width");
			}

		case cast_op::bitcast: {
			if (cn->target_type().is_vector()) {
				switch (cn->target_type().element_width()) {
				case 1:
					return builder.CreateBitCast(val, ::llvm::VectorType::get(types.i1, cn->target_type().nr_elements(), false));
				case 8:
					return builder.CreateBitCast(val, ::llvm::VectorType::get(types.i8, cn->target_type().nr_elements(), false));
				case 16:
					return builder.CreateBitCast(val, ::llvm::VectorType::get(types.i16, cn->target_type().nr_elements(), false));
				case 32:
					return builder.CreateBitCast(val, ::llvm::VectorType::get(types.i32, cn->target_type().nr_elements(), false));
				case 64:
					return builder.CreateBitCast(val, ::llvm::VectorType::get(types.i64, cn->target_type().nr_elements(), false));

				default:
					throw std::runtime_error("unsupported bitcast element width");
				}
			}
			return val;
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

		return builder.CreateSelect(cond, tv, fv);
	}

	case node_kinds::unary_arith: {
		auto un = (unary_arith_node *)n;

		auto v = lower_port(builder, state_arg, pkt, un->lhs());

		switch (un->op()) {
		case unary_arith_op::bnot:
			return builder.CreateNot(v);

		default:
			throw std::runtime_error("unsupported unary operator");
		}
	}

	case node_kinds::bit_shift: {
		auto bsn = (bit_shift_node *)n;

		auto input = lower_port(builder, state_arg, pkt, bsn->input());
		auto amount = lower_port(builder, state_arg, pkt, bsn->amount());

		amount = builder.CreateZExt(amount, input->getType());

		switch (bsn->op()) {
		case shift_op::asr:
			return builder.CreateAShr(input, amount);
		case shift_op::lsr:
			return builder.CreateLShr(input, amount);
		case shift_op::lsl:
			return builder.CreateShl(input, amount);

		default:
			throw std::runtime_error("unsupported shift op");
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
			return ConstantInt::get(types.i8, 0);
			// throw std::runtime_error("unsupported port kind");
		}
	}

	default:
		throw std::runtime_error("unsupported port node kind " + std::to_string((int)n->kind()));
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
		auto current_block = builder.GetInsertBlock();
		auto intermediate_block = BasicBlock::Create(*llvm_context_, "IB", current_block->getParent());
		builder.CreateBr(intermediate_block);
		builder.SetInsertPoint(intermediate_block);

		label_nodes_to_llvm_blocks_[(label_node *)a] = intermediate_block;
		return nullptr;
	}

	case node_kinds::write_reg: {
		auto wrn = (write_reg_node *)a;
		// gep register
		// store value

		// std::cerr << "wreg off=" << wrn->regoff() << std::endl;

		auto dest_reg = builder.CreateGEP(
			types.cpu_state, state_arg, { ConstantInt::get(types.i64, 0), ConstantInt::get(types.i32, wrn->regoff()) }, reg_name(wrn->regoff()));

		auto *reg_type = ((GetElementPtrInst*)dest_reg)->getResultElementType();

		if (auto dest_reg_i = ::llvm::dyn_cast<Instruction>(dest_reg)) {
			dest_reg_i->setMetadata(LLVMContext::MD_alias_scope, MDNode::get(*llvm_context_, reg_file_alias_scope_));
		}

		auto val = lower_port(builder, state_arg, pkt, wrn->value());
                
		//Bitcast the resulting value to the type of the register
		//since the operations can happen on different type after
		//other casting operations.
		auto reg_val = builder.CreateBitCast(val, reg_type);

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
		auto intermediate_block = BasicBlock::Create(*llvm_context_, "IB", current_block->getParent());

		auto cond = lower_port(builder, state_arg, pkt, cbn->cond());

		auto br = builder.CreateCondBr(cond, label_nodes_to_llvm_blocks_[cbn->target()], intermediate_block);

		builder.SetInsertPoint(intermediate_block);

		return br;
	}

	default:
		throw std::runtime_error("unsupported node kind " + std::to_string((int)a->kind()));
	}
}

void llvm_static_output_engine_impl::lower_chunk(SwitchInst *pcswitch, BasicBlock *contblock, std::shared_ptr<chunk> c)
{
	IRBuilder<> builder(*llvm_context_);
	std::map<unsigned long, BasicBlock *> blocks;

	if (c->packets().empty()) {
		return;
	}

	auto state_arg = contblock->getParent()->getArg(0);

	for (auto p : c->packets()) {
		std::stringstream block_name;
		block_name << "INSN_" << std::hex << p->address();

		BasicBlock *block = BasicBlock::Create(*llvm_context_, block_name.str(), contblock->getParent());
		blocks[p->address()] = block;
	}

	BasicBlock *packet_block = nullptr;
	for (auto p : c->packets()) {
		auto next_block = blocks[p->address()];

		if (packet_block != nullptr) {
			builder.CreateBr(next_block);
		}

		packet_block = next_block;

		builder.SetInsertPoint(packet_block);

		if (!p->actions().empty()) {
			for (auto a : p->actions()) {
				lower_node(builder, state_arg, p, a);
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

	for (auto b : blocks) {
		pcswitch->addCase(ConstantInt::get(types.i64, b.first), b.second);
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
	auto TM = T->createTargetMachine(TT, "generic", "", TO, RM);

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
