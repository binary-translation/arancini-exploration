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

	void dump(std::ostream &out) const
	{
		switch (type_) {
#define handle_instr(type, opcode)                                                                                                                             \
	if (type == &Assembler::opcode) {                                                                                                                          \
		out << #opcode;                                                                                                                                        \
	}
#define ehandle_instr(type, opcode) else handle_instr(type, opcode)

		case InstructionType::Dead:
			return;
		case InstructionType::Label:
			if (labelFunc_ == &Assembler::Bind) {
				out << "label" << std::hex << label << ":";
			} else {
				throw std::runtime_error("Unknown label function");
			}
			break;
		case InstructionType::RdImm:
			if (rdImmFunc_ == &Assembler::lui) {
				out << "lui";
			} else if (rdImmFunc_ == &Assembler::auipc) {
				out << "auipc";
			} else {
				throw std::runtime_error("Unknown RdImm function");
			}
			out << " " << std::dec << "xV" << rd.encoding() << ", " << std::hex << "0x" << imm;
			break;
		case InstructionType::RdLabelNear:
		case InstructionType::RdLabelFar:
			if (rdLabelFunc_ == (RdLabelFunc)&Assembler::jal) {
				out << "jal";
			} else {
				throw std::runtime_error("Unknown RdLabel function");
			}
			out << " " << std::dec << "xV" << rd.encoding() << ", " << std::hex << "label" << label;

			break;
		case InstructionType::RdRs1Imm:
			if (rdRs1ImmFunc_ == &Assembler::addi_normal || rdRs1ImmFunc_ == &Assembler::addi_big) {
				out << "addi";
			} else if (rdRs1ImmFunc_ == &Assembler::addiw) {
				out << "addiw";
			} else if (rdRs1ImmFunc_ == &Assembler::andi) {
				out << "andi";
			} else if (rdRs1ImmFunc_ == (RdRs1ImmFunc)&Assembler::jalr) {
				out << "jalr";
			} else if (rdRs1ImmFunc_ == &Assembler::ori) {
				out << "ori";
			} else if (rdRs1ImmFunc_ == &Assembler::slli) {
				out << "slli";
			} else if (rdRs1ImmFunc_ == &Assembler::slliw) {
				out << "slliw";
			} else if (rdRs1ImmFunc_ == &Assembler::slti) {
				out << "slti";
			} else if (rdRs1ImmFunc_ == &Assembler::sltiu) {
				out << "sltiu";
			} else if (rdRs1ImmFunc_ == &Assembler::srai) {
				out << "srai";
			} else if (rdRs1ImmFunc_ == &Assembler::sraiw) {
				out << "sraiw";
			} else if (rdRs1ImmFunc_ == &Assembler::srli) {
				out << "srli";
			} else if (rdRs1ImmFunc_ == &Assembler::srliw) {
				out << "srliw";
			} else if (rdRs1ImmFunc_ == &Assembler::xori) {
				out << "xori";
			} else {
				throw std::runtime_error("Unknown RdRs1Imm function");
			}
			out << " " << std::dec << "xV" << rd.encoding() << ", xV" << rs1.encoding() << ", " << std::hex << "0x" << imm;

			break;
		case InstructionType::Rs1Rs2LabelNear:
		case InstructionType::Rs1Rs2LabelFar:
			if (rs1Rs2LabelBoolFunc_ == (Rs1Rs2LabelBoolFunc)&Assembler::bne) {
				out << "bne";
			} else if (rs1Rs2LabelBoolFunc_ == (Rs1Rs2LabelBoolFunc)&Assembler::beq) {
				out << "beq";
			} else {
				throw std::runtime_error("Unknown Rs1Rs2Label function");
			}
			out << " " << std::dec << "xV" << rs1.encoding() << ", xV" << rs2.encoding() << ", " << std::hex << "label" << label;
			break;
		case InstructionType::Rs1Rs2Label:
			if (rs1Rs2LabelFunc_ == (Rs1Rs2LabelFunc)&Assembler::blt) {
				out << "blt";
			} else if (rs1Rs2LabelFunc_ == (Rs1Rs2LabelFunc)&Assembler::bltu) {
				out << "bltu";
			} else if (rs1Rs2LabelFunc_ == (Rs1Rs2LabelFunc)&Assembler::bge) {
				out << "bge";
			} else if (rs1Rs2LabelFunc_ == (Rs1Rs2LabelFunc)&Assembler::bgeu) {
				out << "bgeu";
			} else {
				throw std::runtime_error("Unknown Rs1Rs2Label function");
			}
			out << " " << std::dec << "xV" << rd.encoding() << ", xV" << rs1.encoding() << ", xV" << rs2.encoding() << ", " << std::hex << "label" << label;
			break;
		case InstructionType::RdAddr:
			if (rdAddrFunc_ == &Assembler::lb) {
				out << "lb";
			} else if (rdAddrFunc_ == &Assembler::lbu) {
				out << "lbu";
			} else if (rdAddrFunc_ == &Assembler::ld) {
				out << "ld";
			} else if (rdAddrFunc_ == &Assembler::lh) {
				out << "lh";
			} else if (rdAddrFunc_ == &Assembler::lhu) {
				out << "lhu";
			} else if (rdAddrFunc_ == &Assembler::lw) {
				out << "lw";
			} else if (rdAddrFunc_ == &Assembler::lwu) {
				out << "lwu";
			} else {
				throw std::runtime_error("Unknown RdAddr function");
			}
			out << " " << std::dec << "xV" << rd.encoding() << ", " << imm << "(xV" << rs1.encoding() << ")";
			break;
		case InstructionType::Rs2Addr:
			if (rdAddrFunc_ == &Assembler::sb) {
				out << "sb";
			} else if (rdAddrFunc_ == &Assembler::sh) {
				out << "sh";
			} else if (rdAddrFunc_ == &Assembler::sw) {
				out << "sw";
			} else if (rdAddrFunc_ == &Assembler::sd) {
				out << "sd";
			} else {
				throw std::runtime_error("Unknown Rs2Addr function");
			}
			out << " " << std::dec << "xV" << rs2.encoding() << ", " << imm << "(xV" << rs1.encoding() << ")";
			break;
		case InstructionType::RdRs1Rs2:
			// clang-format off
			handle_instr(rdRs1Rs2Func_, add)
			ehandle_instr(rdRs1Rs2Func_, addw)
			ehandle_instr(rdRs1Rs2Func_, and_)
			ehandle_instr(rdRs1Rs2Func_, div)
			ehandle_instr(rdRs1Rs2Func_, divu)
			ehandle_instr(rdRs1Rs2Func_, divuw)
			ehandle_instr(rdRs1Rs2Func_, divw)
			ehandle_instr(rdRs1Rs2Func_, mul)
			ehandle_instr(rdRs1Rs2Func_, mulh)
			ehandle_instr(rdRs1Rs2Func_, mulhsu)
			ehandle_instr(rdRs1Rs2Func_, mulhu)
			ehandle_instr(rdRs1Rs2Func_, mulw)
			ehandle_instr(rdRs1Rs2Func_, or_)
			ehandle_instr(rdRs1Rs2Func_, rem)
			ehandle_instr(rdRs1Rs2Func_, remu)
			ehandle_instr(rdRs1Rs2Func_, remuw)
			ehandle_instr(rdRs1Rs2Func_, remw)
			ehandle_instr(rdRs1Rs2Func_, sll)
			ehandle_instr(rdRs1Rs2Func_, sllw)
			ehandle_instr(rdRs1Rs2Func_, slt)
			ehandle_instr(rdRs1Rs2Func_, sltu)
			ehandle_instr(rdRs1Rs2Func_, sra)
			ehandle_instr(rdRs1Rs2Func_, sraw)
			ehandle_instr(rdRs1Rs2Func_, srl)
			ehandle_instr(rdRs1Rs2Func_, srlw)
			ehandle_instr(rdRs1Rs2Func_, sub)
			ehandle_instr(rdRs1Rs2Func_, subw)
			ehandle_instr(rdRs1Rs2Func_, xor_)
			else {
				// clang-format on
				throw std::runtime_error("Unknown RdRs1Rs2 function");
			}
			out << " " << std::dec << "xV" << rd.encoding() << ", xV" << rs1.encoding() << ", xV" << rs2.encoding();
			break;
		case InstructionType::None:
			// clang-format off
			handle_instr(noneFunc_, ebreak)
			ehandle_instr(noneFunc_, ecall)
			ehandle_instr(noneFunc_, fencei)
			ehandle_instr(noneFunc_, trap)
			else {
				// clang-format on
				throw std::runtime_error("Unknown None function");
			}
			break;
		case InstructionType::RdAddrOrder:
			// clang-format off
			handle_instr(rdAddrOrderFunc_, lrw)
			ehandle_instr(rdAddrOrderFunc_, lrd)
			else {
				// clang-format on
				throw std::runtime_error("Unknown RdAddrOrder function");
			}

			switch (order) {
			case std::memory_order_relaxed:
			case std::memory_order_consume:
			case std::memory_order_seq_cst:
				break;
			case std::memory_order_acquire:
				out << ".aq";
				break;
			case std::memory_order_release:
				out << ".rl";
				break;
			case std::memory_order_acq_rel:
				out << ".aq.rl";
				break;
			}
			out << " " << std::dec << "xV" << rd.encoding() << ", (xV" << rs1.encoding() << ")";
			break;
		case InstructionType::RdRs2AddrOrder:
			// clang-format off
			handle_instr(rdRs2AddrOrderFunc_, amoaddd)
			ehandle_instr(rdRs2AddrOrderFunc_, amoaddw)
			ehandle_instr(rdRs2AddrOrderFunc_, amoandd)
			ehandle_instr(rdRs2AddrOrderFunc_, amoandw)
			ehandle_instr(rdRs2AddrOrderFunc_, amomaxd)
			ehandle_instr(rdRs2AddrOrderFunc_, amomaxw)
			ehandle_instr(rdRs2AddrOrderFunc_, amomaxud)
			ehandle_instr(rdRs2AddrOrderFunc_, amomaxuw)
			ehandle_instr(rdRs2AddrOrderFunc_, amomind)
			ehandle_instr(rdRs2AddrOrderFunc_, amominw)
			ehandle_instr(rdRs2AddrOrderFunc_, amominud)
			ehandle_instr(rdRs2AddrOrderFunc_, amominuw)
			ehandle_instr(rdRs2AddrOrderFunc_, amoord)
			ehandle_instr(rdRs2AddrOrderFunc_, amoorw)
			ehandle_instr(rdRs2AddrOrderFunc_, amoxord)
			ehandle_instr(rdRs2AddrOrderFunc_, amoxorw)
			ehandle_instr(rdRs2AddrOrderFunc_, amoswapd)
			ehandle_instr(rdRs2AddrOrderFunc_, amoswapw)
			ehandle_instr(rdRs2AddrOrderFunc_, scd)
			ehandle_instr(rdRs2AddrOrderFunc_, scw)
			else {
				// clang-format on
				throw std::runtime_error("Unknown RdRs2AddrOrder function");
			}

			switch (order) {
			case std::memory_order_relaxed:
			case std::memory_order_consume:
			case std::memory_order_seq_cst:
				break;
			case std::memory_order_acquire:
				out << ".aq";
				break;
			case std::memory_order_release:
				out << ".rl";
				break;
			case std::memory_order_acq_rel:
				out << ".aq.rl";
				break;
			}
			out << " " << std::dec << "xV" << rd.encoding() << ", xV" << rs2.encoding() << ", (xV" << rs1.encoding() << ")";
			break;
		case InstructionType::RdImmKeepRs1:
			// clang-format off
			handle_instr(rdImmFunc_, auipc)
			else {
				// clang-format on
				throw std::runtime_error("Unknown RdImmKeepRs1 function");
			}
			out << " " << std::dec << "xV" << rd.encoding() << ", " << std::hex << "0x" << imm << std::dec << ", keep xV" << rs1.encoding();
			break;
		case InstructionType::RdRs1ImmKeepRs2:
			if (rdRs1ImmFunc_ == &Assembler::addi_normal) {
				out << "addi";
			} else {
				throw std::runtime_error("Unknown RdRs1ImmKeepRs2 function");
			}
			out << " " << std::dec << "xV" << rd.encoding() << ", xV" << rs1.encoding() << ", " << std::hex << "0x" << imm << std::dec << ", keep xV"
				<< rs2.encoding();
			break;
		}
#undef handle_instr
#undef ehandle_instr
	}

	bool is_control_flow()
	{
		switch (type_) {

		case InstructionType::Label:
		case InstructionType::RdLabelNear:
		case InstructionType::RdLabelFar:
		case InstructionType::Rs1Rs2LabelNear:
		case InstructionType::Rs1Rs2LabelFar:
		case InstructionType::Rs1Rs2Label:
			return true;
		case InstructionType::Dead:
		case InstructionType::RdImm:
		case InstructionType::RdRs1Imm:
		case InstructionType::RdAddr:
		case InstructionType::Rs2Addr:
		case InstructionType::RdRs1Rs2:
		case InstructionType::None:
		case InstructionType::RdAddrOrder:
		case InstructionType::RdRs2AddrOrder:
		case InstructionType::RdImmKeepRs1:
		case InstructionType::RdRs1ImmKeepRs2:
			return false;
		}
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