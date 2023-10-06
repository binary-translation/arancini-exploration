#pragma once

#include <arancini/output/dynamic/riscv64/instruction-builder/address-operand.h>
#include <arancini/output/dynamic/riscv64/instruction-builder/register-operand.h>
#include <cstdint>

namespace arancini::output::dynamic::riscv64::builder {
static const bool kNearJump = true;
static const bool kFarJump = false;

enum class InstructionType {
	Dead,
	Label,
	RdImm,
	RdLabelNear,
	RdLabelFar,
	RdRs1Imm,
	Rs1Rs2LabelNear,
	Rs1Rs2LabelFar,
	Rs1Rs2Label,
	RdAddr,
	Rs2Addr,
	RdRs1Rs2,
	None,
	RdAddrOrder,
	RdRs2AddrOrder,
	RdImmKeepRs1,
	RdRs1ImmKeepRs2
};

using LabelFunc = decltype(&Assembler::Bind);
using RdImmFunc = decltype(&Assembler::lui);
using RdLabelFunc = void (Assembler::*)(Register, Label *, bool);
using RdRs1ImmFunc = decltype(&Assembler::xori);
using Rs1Rs2LabelBoolFunc = void (Assembler::*)(Register, Register, Label *, bool);
using Rs1Rs2LabelFunc = void (Assembler::*)(Register, Register, Label *);
using RdAddrFunc = decltype(&Assembler::lb);
using RdRs1Rs2Func = decltype(&Assembler::add);
using NoneFunc = decltype(&Assembler::fencei);
using RdAddrOrderFunc = decltype(&Assembler::lrw);
using RdRs2AddrOrderFunc = decltype(&Assembler::scw);

struct Instruction {
	Instruction(const InstructionType type, const LabelFunc labelFunc, Label *label)
		: rd(none_reg)
		, rs1(none_reg)
		, rs2(none_reg)
		, label(label)
		, type_(type)
		, labelFunc_(labelFunc)
	{
		ASSERT(!(!rd && has_rd()));
	}
	Instruction(const InstructionType type, const RdImmFunc rdImmFunc, const RegisterOperand rd, const intptr_t imm)
		: rd(rd)
		, rs1(none_reg)
		, rs2(none_reg)
		, imm(imm)
		, type_(type)
		, rdImmFunc_(rdImmFunc)
	{
		ASSERT(!(!rd && has_rd()));
	}
	Instruction(const InstructionType type, const RdImmFunc rdImmFunc, const RegisterOperand rd, const RegisterOperand rs1, const intptr_t imm)
		: rd(rd)
		, rs1(rs1)
		, rs2(none_reg)
		, imm(imm)
		, type_(type)
		, rdImmFunc_(rdImmFunc)
	{
		ASSERT(!(!rd && has_rd()));
	}
	Instruction(const InstructionType type, const RdLabelFunc rdLabelFunc, const RegisterOperand rd, Label *label)
		: rd(rd)
		, rs1(none_reg)
		, rs2(none_reg)
		, label(label)
		, type_(type)
		, rdLabelFunc_(rdLabelFunc)
	{
		ASSERT(!(!rd && has_rd()));
	}
	Instruction(const InstructionType type, const RdRs1ImmFunc rdRs1ImmFunc, const RegisterOperand rd, const RegisterOperand rs1, const intptr_t imm)
		: rd(rd)
		, rs1(rs1)
		, rs2(none_reg)
		, imm(imm)
		, type_(type)
		, rdRs1ImmFunc_(rdRs1ImmFunc)
	{
		ASSERT(!(!rd && has_rd()));
	}
	Instruction(const InstructionType type, const RdRs1ImmFunc rdRs1ImmFunc, const RegisterOperand rd, const RegisterOperand rs1, const RegisterOperand rs2,
		const intptr_t imm)
		: rd(rd)
		, rs1(rs1)
		, rs2(rs2)
		, imm(imm)
		, type_(type)
		, rdRs1ImmFunc_(rdRs1ImmFunc)
	{
		ASSERT(!(!rd && has_rd()));
	}
	Instruction(const InstructionType type, const Rs1Rs2LabelBoolFunc rs1Rs2LabelBoolFunc, const RegisterOperand rs1, const RegisterOperand rs2, Label *label)
		: rd(none_reg)
		, rs1(rs1)
		, rs2(rs2)
		, label(label)
		, type_(type)
		, rs1Rs2LabelBoolFunc_(rs1Rs2LabelBoolFunc)
	{
		ASSERT(!(!rd && has_rd()));
	}
	Instruction(const InstructionType type, const Rs1Rs2LabelFunc rs1Rs2LabelFunc, const RegisterOperand rs1, const RegisterOperand rs2, Label *label)
		: rd(none_reg)
		, rs1(rs1)
		, rs2(rs2)
		, label(label)
		, type_(type)
		, rs1Rs2LabelFunc_(rs1Rs2LabelFunc)
	{
		ASSERT(!(!rd && has_rd()));
	}
	Instruction(const InstructionType type, const RdAddrFunc rdAddrFunc, const RegisterOperand rd, const RegisterOperand rs1, const RegisterOperand rs2,
		const intptr_t imm)
		: rd(rd)
		, rs1(rs1)
		, rs2(rs2)
		, imm(imm)
		, type_(type)
		, rdAddrFunc_(rdAddrFunc)
	{
		ASSERT(!(!rd && has_rd()));
	}
	Instruction(
		const InstructionType type, const RdAddrOrderFunc rdAddrOrderFunc, const RegisterOperand rd, const RegisterOperand rs1, const std::memory_order order)
		: rd(rd)
		, rs1(rs1)
		, rs2(none_reg)
		, order(order)
		, type_(type)
		, rdAddrOrderFunc_(rdAddrOrderFunc)
	{
		ASSERT(!(!rd && has_rd()));
	}
	Instruction(const InstructionType type, const RdRs2AddrOrderFunc rdRs2AddrOrderFunc, const RegisterOperand rd, const RegisterOperand rs1,
		const RegisterOperand rs2, const std::memory_order order)
		: rd(rd)
		, rs1(rs1)
		, rs2(rs2)
		, order(order)
		, type_(type)
		, rdRs2AddrOrderFunc_(rdRs2AddrOrderFunc)
	{
		ASSERT(!(!rd && has_rd()));
	}
	Instruction(const InstructionType type, const RdRs1Rs2Func rdRs1Rs2Func, const RegisterOperand rd, const RegisterOperand rs1, const RegisterOperand rs2)
		: rd(rd)
		, rs1(rs1)
		, rs2(rs2)
		, imm(0)
		, type_(type)
		, rdRs1Rs2Func_(rdRs1Rs2Func)
	{
		ASSERT(!(!rd && has_rd()));
	}
	Instruction(const InstructionType type, const NoneFunc noneFunc)
		: rd(none_reg)
		, rs1(none_reg)
		, rs2(none_reg)
		, imm(0)
		, type_(type)
		, noneFunc_(noneFunc)
	{
		ASSERT(!(!rd && has_rd()));
	}

	void emit(Assembler &assembler) const
	{
		switch (type_) {
		case InstructionType::Label:
			(assembler.*labelFunc_)(label);
			break;
		case InstructionType::RdImm:
		case InstructionType::RdImmKeepRs1:
			(assembler.*rdImmFunc_)(rd, imm);
			break;
		case InstructionType::RdLabelNear:
			(assembler.*rdLabelFunc_)(rd, label, kNearJump);
			break;
		case InstructionType::RdLabelFar:
			(assembler.*rdLabelFunc_)(rd, label, kFarJump);
			break;
		case InstructionType::RdRs1Imm:
		case InstructionType::RdRs1ImmKeepRs2:
			(assembler.*rdRs1ImmFunc_)(rd, rs1, imm);
			break;
		case InstructionType::Rs1Rs2LabelNear:
			(assembler.*rs1Rs2LabelBoolFunc_)(rs1, rs2, label, kNearJump);
			break;
		case InstructionType::Rs1Rs2LabelFar:
			(assembler.*rs1Rs2LabelBoolFunc_)(rs1, rs2, label, kFarJump);
			break;
		case InstructionType::Rs1Rs2Label:
			(assembler.*rs1Rs2LabelFunc_)(rs1, rs2, label);
			break;
		case InstructionType::RdAddr:
			(assembler.*rdAddrFunc_)(rd, Address { rs1, imm });
			break;
		case InstructionType::Rs2Addr:
			(assembler.*rdAddrFunc_)(rs2, Address { rs1, imm });
			break;
		case InstructionType::RdRs1Rs2:
			(assembler.*rdRs1Rs2Func_)(rd, rs1, rs2);
			break;
		case InstructionType::None:
			(assembler.*noneFunc_)();
			break;
		case InstructionType::RdAddrOrder:
			(assembler.*rdAddrOrderFunc_)(rd, Address { rs1 }, order);
			break;
		case InstructionType::RdRs2AddrOrder:
			(assembler.*rdRs2AddrOrderFunc_)(rd, rs2, Address { rs1 }, order);
			break;
		case InstructionType::Dead:
			break;
		}
	}

	[[nodiscard]] bool has_rd() const
	{
		switch (type_) {

		case InstructionType::Dead:
		case InstructionType::Label:
		case InstructionType::Rs1Rs2LabelNear:
		case InstructionType::Rs1Rs2LabelFar:
		case InstructionType::Rs1Rs2Label:
		case InstructionType::Rs2Addr:
		case InstructionType::None:
			return false;
		case InstructionType::RdImm:
		case InstructionType::RdLabelNear:
		case InstructionType::RdLabelFar:
		case InstructionType::RdRs1Imm:
		case InstructionType::RdAddr:
		case InstructionType::RdRs1Rs2:
		case InstructionType::RdAddrOrder:
		case InstructionType::RdRs2AddrOrder:
		case InstructionType::RdImmKeepRs1:
		case InstructionType::RdRs1ImmKeepRs2:
			return true;
		}
		return false;
	}

	[[nodiscard]] bool is_rd_use() const
	{
		switch (type_) {

		case InstructionType::Label:
		case InstructionType::RdImm:
		case InstructionType::RdLabelNear:
		case InstructionType::RdLabelFar:
		case InstructionType::Rs1Rs2LabelNear:
		case InstructionType::Rs1Rs2LabelFar:
		case InstructionType::Rs1Rs2Label:
		case InstructionType::Rs2Addr:
		case InstructionType::None:
		case InstructionType::Dead:
			return false;
		case InstructionType::RdRs1Imm:
		case InstructionType::RdAddr:
		case InstructionType::RdAddrOrder:
		case InstructionType::RdImmKeepRs1:
			return rd == rs1;
		case InstructionType::RdRs1Rs2:
		case InstructionType::RdRs2AddrOrder:
		case InstructionType::RdRs1ImmKeepRs2:
			return rd == rs1 || rd == rs2;
		}
		return false;
	}

	void kill() { type_ = InstructionType::Dead; }

	[[nodiscard]] bool is_dead() const { return type_ == InstructionType::Dead; }

	bool is_mv()
	{
		return ((type_ == InstructionType::RdRs1ImmKeepRs2 || type_ == InstructionType::RdRs1Imm) && rdRs1ImmFunc_ == &Assembler::addi_normal && imm == 0)
			|| (type_ == InstructionType::RdRs1Rs2 && rdRs1Rs2Func_ == &Assembler::add && (rs1 == ZERO || rs2 == ZERO));
	}

	RegisterOperand rd, rs1, rs2;

	union {
		const intptr_t imm;
		Label *label;
		const std::memory_order order;
	};

private:
	InstructionType type_;
	union {
		const LabelFunc labelFunc_;
		const RdImmFunc rdImmFunc_;
		const RdLabelFunc rdLabelFunc_;
		const RdRs1ImmFunc rdRs1ImmFunc_;
		const Rs1Rs2LabelBoolFunc rs1Rs2LabelBoolFunc_;
		const Rs1Rs2LabelFunc rs1Rs2LabelFunc_;
		const RdAddrFunc rdAddrFunc_;
		const RdRs1Rs2Func rdRs1Rs2Func_;
		const NoneFunc noneFunc_;
		const RdAddrOrderFunc rdAddrOrderFunc_;
		const RdRs2AddrOrderFunc rdRs2AddrOrderFunc_;
	};
};
} // namespace arancini::output::dynamic::riscv64::builder