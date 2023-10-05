#pragma once

#include <arancini/output/dynamic/riscv64/instruction-builder/instruction.h>

#include <bitset>
#include <unordered_map>

namespace arancini::output::dynamic::riscv64::builder {

class InstructionBuilder {
public:
	void Bind(Label *label) { instructions_.emplace_back(InstructionType::Label, &Assembler::Bind, label); }

	// ==== RV32I ====
	void lui(RegisterOperand rd, intptr_t imm) { instructions_.emplace_back(InstructionType::RdImm, &Assembler::lui, rd, imm); }
	void auipc(RegisterOperand rd, intptr_t imm) { instructions_.emplace_back(InstructionType::RdImm, &Assembler::auipc, rd, imm); }
	void auipc_keep(RegisterOperand rd, intptr_t imm) { instructions_.emplace_back(InstructionType::RdImmKeepRs1, &Assembler::auipc, rd, rd, imm); }

	void jal(RegisterOperand rd, Label *label, bool near = kFarJump)
	{
		instructions_.emplace_back(near ? InstructionType::RdLabelNear : InstructionType::RdLabelFar, (RdLabelFunc)&Assembler::jal, rd, label);
	}
	void jal(Label *label, bool near = kFarJump) { jal(RA, label, near); }
	void j(Label *label, bool near = kFarJump) { jal(ZERO, label, near); }

	void jalr(RegisterOperand rd, RegisterOperand rs1, intptr_t offset = 0)
	{
		instructions_.emplace_back(InstructionType::RdRs1Imm, (RdRs1ImmFunc)&Assembler::jalr, rd, rs1, offset);
	}
	void jalr(RegisterOperand rs1, intptr_t offset = 0) { jalr(RA, rs1, offset); }
	void jr(RegisterOperand rs1, intptr_t offset = 0) { jalr(ZERO, rs1, offset); }
	void ret() { jalr(ZERO, RA, 0); }

	void beq(RegisterOperand rs1, RegisterOperand rs2, Label *label, bool near = kFarJump)
	{
		instructions_.emplace_back(
			near ? InstructionType::Rs1Rs2LabelNear : InstructionType::Rs1Rs2LabelFar, (Rs1Rs2LabelBoolFunc)&Assembler::beq, rs1, rs2, label);
	}
	void bne(RegisterOperand rs1, RegisterOperand rs2, Label *label, bool near = kFarJump)
	{
		instructions_.emplace_back(
			near ? InstructionType::Rs1Rs2LabelNear : InstructionType::Rs1Rs2LabelFar, (Rs1Rs2LabelBoolFunc)&Assembler::bne, rs1, rs2, label);
	}
	void blt(RegisterOperand rs1, RegisterOperand rs2, Label *label)
	{
		instructions_.emplace_back(InstructionType::Rs1Rs2Label, (Rs1Rs2LabelFunc)&Assembler::blt, rs1, rs2, label);
	}
	void bge(RegisterOperand rs1, RegisterOperand rs2, Label *label)
	{
		instructions_.emplace_back(InstructionType::Rs1Rs2Label, (Rs1Rs2LabelFunc)&Assembler::bge, rs1, rs2, label);
	}
	void bgt(RegisterOperand rs1, RegisterOperand rs2, Label *label) { blt(rs2, rs1, label); }
	void ble(RegisterOperand rs1, RegisterOperand rs2, Label *label) { bge(rs2, rs1, label); }
	void bltu(RegisterOperand rs1, RegisterOperand rs2, Label *label)
	{
		instructions_.emplace_back(InstructionType::Rs1Rs2Label, (Rs1Rs2LabelFunc)&Assembler::bltu, rs1, rs2, label);
	}
	void bgeu(RegisterOperand rs1, RegisterOperand rs2, Label *label)
	{
		instructions_.emplace_back(InstructionType::Rs1Rs2Label, (Rs1Rs2LabelFunc)&Assembler::bgeu, rs1, rs2, label);
	}
	void bgtu(RegisterOperand rs1, RegisterOperand rs2, Label *label) { bltu(rs2, rs1, label); }
	void bleu(RegisterOperand rs1, RegisterOperand rs2, Label *label) { bgeu(rs2, rs1, label); }

	/*
		// Skipped for now
		intptr_t offset_from_target(intptr_t target);

		void jal(RegisterOperand rd, intptr_t offset);
		void jal(intptr_t offset) { jal(RA, offset); }
		void j(intptr_t offset) { jal(ZERO, offset); }

		void beq(RegisterOperand rs1, RegisterOperand rs2, intptr_t offset);
		void bne(RegisterOperand rs1, RegisterOperand rs2, intptr_t offset);
		void blt(RegisterOperand rs1, RegisterOperand rs2, intptr_t offset);
		void bge(RegisterOperand rs1, RegisterOperand rs2, intptr_t offset);
		void bgt(RegisterOperand rs1, RegisterOperand rs2, intptr_t offset) { blt(rs2, rs1, offset); }
		void ble(RegisterOperand rs1, RegisterOperand rs2, intptr_t offset) { bge(rs2, rs1, offset); }
		void bltu(RegisterOperand rs1, RegisterOperand rs2, intptr_t offset);
		void bgeu(RegisterOperand rs1, RegisterOperand rs2, intptr_t offset);
		void bgtu(RegisterOperand rs1, RegisterOperand rs2, intptr_t offset) { bltu(rs2, rs1, offset); }
		void bleu(RegisterOperand rs1, RegisterOperand rs2, intptr_t offset) { bgeu(rs2, rs1, offset); }

	*/

	void lb(RegisterOperand rd, AddressOperand addr)
	{
		instructions_.emplace_back(InstructionType::RdAddr, &Assembler::lb, rd, addr.base(), none_reg, addr.offset());
	}
	void lh(RegisterOperand rd, AddressOperand addr)
	{
		instructions_.emplace_back(InstructionType::RdAddr, &Assembler::lh, rd, addr.base(), none_reg, addr.offset());
	}
	void lw(RegisterOperand rd, AddressOperand addr)
	{
		instructions_.emplace_back(InstructionType::RdAddr, &Assembler::lw, rd, addr.base(), none_reg, addr.offset());
	}
	void lbu(RegisterOperand rd, AddressOperand addr)
	{
		instructions_.emplace_back(InstructionType::RdAddr, &Assembler::lbu, rd, addr.base(), none_reg, addr.offset());
	}
	void lhu(RegisterOperand rd, AddressOperand addr)
	{
		instructions_.emplace_back(InstructionType::RdAddr, &Assembler::lhu, rd, addr.base(), none_reg, addr.offset());
	}

	void sb(RegisterOperand rs2, AddressOperand addr)
	{
		instructions_.emplace_back(InstructionType::Rs2Addr, &Assembler::sb, none_reg, addr.base(), rs2, addr.offset());
	}
	void sh(RegisterOperand rs2, AddressOperand addr)
	{
		instructions_.emplace_back(InstructionType::Rs2Addr, &Assembler::sh, none_reg, addr.base(), rs2, addr.offset());
	}
	void sw(RegisterOperand rs2, AddressOperand addr)
	{
		instructions_.emplace_back(InstructionType::Rs2Addr, &Assembler::sw, none_reg, addr.base(), rs2, addr.offset());
	}

	void addi(RegisterOperand rd, RegisterOperand rs1, intptr_t imm, bool force_big = false)
	{
		instructions_.emplace_back(InstructionType::RdRs1Imm, force_big ? &Assembler::addi_big : &Assembler::addi_normal, rd, rs1, imm);
	}
	void subi(RegisterOperand rd, RegisterOperand rs1, intptr_t imm) { addi(rd, rs1, -imm); }
	void slti(RegisterOperand rd, RegisterOperand rs1, intptr_t imm) { instructions_.emplace_back(InstructionType::RdRs1Imm, &Assembler::slti, rd, rs1, imm); }
	void sltiu(RegisterOperand rd, RegisterOperand rs1, intptr_t imm)
	{
		instructions_.emplace_back(InstructionType::RdRs1Imm, &Assembler::sltiu, rd, rs1, imm);
	}
	void xori(RegisterOperand rd, RegisterOperand rs1, intptr_t imm) { instructions_.emplace_back(InstructionType::RdRs1Imm, &Assembler::xori, rd, rs1, imm); }
	void ori(RegisterOperand rd, RegisterOperand rs1, intptr_t imm) { instructions_.emplace_back(InstructionType::RdRs1Imm, &Assembler::ori, rd, rs1, imm); }
	void andi(RegisterOperand rd, RegisterOperand rs1, intptr_t imm) { instructions_.emplace_back(InstructionType::RdRs1Imm, &Assembler::andi, rd, rs1, imm); }
	void slli(RegisterOperand rd, RegisterOperand rs1, intptr_t shamt)
	{
		instructions_.emplace_back(InstructionType::RdRs1Imm, &Assembler::slli, rd, rs1, shamt);
	}
	void srli(RegisterOperand rd, RegisterOperand rs1, intptr_t shamt)
	{
		instructions_.emplace_back(InstructionType::RdRs1Imm, &Assembler::srli, rd, rs1, shamt);
	}
	void srai(RegisterOperand rd, RegisterOperand rs1, intptr_t shamt)
	{
		instructions_.emplace_back(InstructionType::RdRs1Imm, &Assembler::srai, rd, rs1, shamt);
	}

	void add(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::add, rd, rs1, rs2);
	}
	void sub(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::sub, rd, rs1, rs2);
	}
	void sll(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::sll, rd, rs1, rs2);
	}
	void slt(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::slt, rd, rs1, rs2);
	}
	void sltu(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::sltu, rd, rs1, rs2);
	}
	void xor_(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::xor_, rd, rs1, rs2);
	}
	void srl(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::srl, rd, rs1, rs2);
	}
	void sra(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::sra, rd, rs1, rs2);
	}
	void or_(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::or_, rd, rs1, rs2);
	}
	void and_(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::and_, rd, rs1, rs2);
	}
	/*
		// Skip for now
		void fence(HartEffects predecessor, HartEffects successor);
		void fence() { fence(kAll, kAll); }

	*/
	void fencei() { instructions_.emplace_back(InstructionType::None, &Assembler::fencei); }
	void ecall() { instructions_.emplace_back(InstructionType::None, &Assembler::ecall); }
	void ebreak() { instructions_.emplace_back(InstructionType::None, &Assembler::ebreak); }

	/*
		// Skip these for now
		void csrrw(RegisterOperand rd, uint32_t csr, RegisterOperand rs1);
		void csrrs(RegisterOperand rd, uint32_t csr, RegisterOperand rs1);
		void csrrc(RegisterOperand rd, uint32_t csr, RegisterOperand rs1);
		void csrr(RegisterOperand rd, uint32_t csr) { csrrs(rd, csr, ZERO); }
		void csrw(uint32_t csr, RegisterOperand rs) { csrrw(ZERO, csr, rs); }
		void csrs(uint32_t csr, RegisterOperand rs) { csrrs(ZERO, csr, rs); }
		void csrc(uint32_t csr, RegisterOperand rs) { csrrc(ZERO, csr, rs); }
		void csrrwi(RegisterOperand rd, uint32_t csr, uint32_t imm);
		void csrrsi(RegisterOperand rd, uint32_t csr, uint32_t imm);
		void csrrci(RegisterOperand rd, uint32_t csr, uint32_t imm);
		void csrwi(uint32_t csr, uint32_t imm) { csrrwi(ZERO, csr, imm); }
		void csrsi(uint32_t csr, uint32_t imm) { csrrsi(ZERO, csr, imm); }
		void csrci(uint32_t csr, uint32_t imm) { csrrci(ZERO, csr, imm); }

	*/
	void trap() { instructions_.emplace_back(InstructionType::None, &Assembler::trap); } // Permanently reserved illegal instruction.

	void nop(bool force_big = false) { addi(ZERO, ZERO, 0, force_big); }
	void li(RegisterOperand rd, intptr_t imm) { addi(rd, ZERO, imm); }
	void mv(RegisterOperand rd, RegisterOperand rs) { addi(rd, rs, 0); }
	/// MV but keeps dependency on previous Rd
	void mv_keep(RegisterOperand rd, RegisterOperand rs)
	{
		instructions_.emplace_back(InstructionType::RdRs1ImmKeepRs2, &Assembler::addi_normal, rd, rs, rd, 0);
	}
	void not_(RegisterOperand rd, RegisterOperand rs) { xori(rd, rs, -1); }
	void neg(RegisterOperand rd, RegisterOperand rs) { sub(rd, ZERO, rs); }

	void snez(RegisterOperand rd, RegisterOperand rs) { sltu(rd, ZERO, rs); }
	void seqz(RegisterOperand rd, RegisterOperand rs) { sltiu(rd, rs, 1); }
	void sltz(RegisterOperand rd, RegisterOperand rs) { slt(rd, rs, ZERO); }
	void sgtz(RegisterOperand rd, RegisterOperand rs) { slt(rd, ZERO, rs); }

	void beqz(RegisterOperand rs, Label *label, bool near = kFarJump) { beq(rs, ZERO, label, near); }
	void bnez(RegisterOperand rs, Label *label, bool near = kFarJump) { bne(rs, ZERO, label, near); }
	void blez(RegisterOperand rs, Label *label) { bge(ZERO, rs, label); }
	void bgez(RegisterOperand rs, Label *label) { bge(rs, ZERO, label); }
	void bltz(RegisterOperand rs, Label *label) { blt(rs, ZERO, label); }
	void bgtz(RegisterOperand rs, Label *label) { blt(ZERO, rs, label); }

/*
	// Not used for now
	void beqz(RegisterOperand rs, intptr_t offset) { beq(rs, ZERO, offset); }
	void bnez(RegisterOperand rs, intptr_t offset) { bne(rs, ZERO, offset); }
	void blez(RegisterOperand rs, intptr_t offset) { bge(ZERO, rs, offset); }
	void bgez(RegisterOperand rs, intptr_t offset) { bge(rs, ZERO, offset); }
	void bltz(RegisterOperand rs, intptr_t offset) { blt(rs, ZERO, offset); }
	void bgtz(RegisterOperand rs, intptr_t offset) { blt(ZERO, rs, offset); }

*/
// ==== RV64I ====
#if XLEN >= 64
	void lwu(RegisterOperand rd, AddressOperand addr)
	{
		instructions_.emplace_back(InstructionType::RdAddr, &Assembler::lwu, rd, addr.base(), none_reg, addr.offset());
	}
	void ld(RegisterOperand rd, AddressOperand addr)
	{
		instructions_.emplace_back(InstructionType::RdAddr, &Assembler::ld, rd, addr.base(), none_reg, addr.offset());
	}

	void sd(RegisterOperand rs2, AddressOperand addr)
	{
		instructions_.emplace_back(InstructionType::Rs2Addr, &Assembler::sd, none_reg, addr.base(), rs2, addr.offset());
	}

	void addiw(RegisterOperand rd, RegisterOperand rs1, intptr_t imm)
	{
		instructions_.emplace_back(InstructionType::RdRs1Imm, &Assembler::addiw, rd, rs1, imm);
	}
	void subiw(RegisterOperand rd, RegisterOperand rs1, intptr_t imm) { addi(rd, rs1, -imm); }
	void slliw(RegisterOperand rd, RegisterOperand rs1, intptr_t shamt)
	{
		instructions_.emplace_back(InstructionType::RdRs1Imm, &Assembler::slliw, rd, rs1, shamt);
	}
	void srliw(RegisterOperand rd, RegisterOperand rs1, intptr_t shamt)
	{
		instructions_.emplace_back(InstructionType::RdRs1Imm, &Assembler::srliw, rd, rs1, shamt);
	}
	void sraiw(RegisterOperand rd, RegisterOperand rs1, intptr_t shamt)
	{
		instructions_.emplace_back(InstructionType::RdRs1Imm, &Assembler::sraiw, rd, rs1, shamt);
	}

	void addw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::addw, rd, rs1, rs2);
	}
	void subw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::subw, rd, rs1, rs2);
	}
	void sllw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::sllw, rd, rs1, rs2);
	}
	void srlw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::srlw, rd, rs1, rs2);
	}
	void sraw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::sraw, rd, rs1, rs2);
	}

	void negw(RegisterOperand rd, RegisterOperand rs) { subw(rd, ZERO, rs); }
	void sextw(RegisterOperand rd, RegisterOperand rs) { addiw(rd, rs, 0); }
#endif // XLEN >= 64

#if XLEN == 32
	void lx(RegisterOperand rd, AddressOperand addr) { lw(rd, addr); }
	void sx(RegisterOperand rs2, AddressOperand addr) { sw(rs2, addr); }
#elif XLEN == 64
	void lx(RegisterOperand rd, AddressOperand addr) { ld(rd, addr); }
	void sx(RegisterOperand rs2, AddressOperand addr) { sd(rs2, addr); }
#elif XLEN == 128
	void lx(RegisterOperand rd, AddressOperand addr) { lq(rd, addr); }
	void sx(RegisterOperand rs2, AddressOperand addr) { sq(rs2, addr); }
#endif

	// ==== RV32M ====
	void mul(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::mul, rd, rs1, rs2);
	}
	void mulh(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::mulh, rd, rs1, rs2);
	}
	void mulhsu(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::mulhsu, rd, rs1, rs2);
	}
	void mulhu(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::mulhu, rd, rs1, rs2);
	}
	void div(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::div, rd, rs1, rs2);
	}
	void divu(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::divu, rd, rs1, rs2);
	}
	void rem(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::rem, rd, rs1, rs2);
	}
	void remu(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::remu, rd, rs1, rs2);
	}

// ==== RV64M ====
#if XLEN >= 64
	void mulw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::mulw, rd, rs1, rs2);
	}
	void divw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::divw, rd, rs1, rs2);
	}
	void divuw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::divuw, rd, rs1, rs2);
	}
	void remw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::remw, rd, rs1, rs2);
	}
	void remuw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2)
	{
		instructions_.emplace_back(InstructionType::RdRs1Rs2, &Assembler::remuw, rd, rs1, rs2);
	}
#endif // XLEN >= 64

	// ==== RV32A ====
	void lrw(RegisterOperand rd, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdAddrOrder, &Assembler::lrw, rd, addr.base(), order);
	}
	void scw(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::scw, rd, addr.base(), rs2, order);
	}
	void amoswapw(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amoswapw, rd, addr.base(), rs2, order);
	}
	void amoaddw(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amoaddw, rd, addr.base(), rs2, order);
	}
	void amoxorw(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amoxorw, rd, addr.base(), rs2, order);
	}
	void amoandw(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amoandw, rd, addr.base(), rs2, order);
	}
	void amoorw(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amoorw, rd, addr.base(), rs2, order);
	}
	void amominw(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amominw, rd, addr.base(), rs2, order);
	}
	void amomaxw(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amomaxw, rd, addr.base(), rs2, order);
	}
	void amominuw(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amominuw, rd, addr.base(), rs2, order);
	}
	void amomaxuw(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amomaxuw, rd, addr.base(), rs2, order);
	}

// ==== RV64A ====
#if XLEN >= 64
	void lrd(RegisterOperand rd, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdAddrOrder, &Assembler::lrd, rd, addr.base(), order);
	}
	void scd(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::scd, rd, addr.base(), rs2, order);
	}
	void amoswapd(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amoswapd, rd, addr.base(), rs2, order);
	}
	void amoaddd(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amoaddd, rd, addr.base(), rs2, order);
	}
	void amoxord(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amoxord, rd, addr.base(), rs2, order);
	}
	void amoandd(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amoandd, rd, addr.base(), rs2, order);
	}
	void amoord(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amoord, rd, addr.base(), rs2, order);
	}
	void amomind(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amomind, rd, addr.base(), rs2, order);
	}
	void amomaxd(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amomaxd, rd, addr.base(), rs2, order);
	}
	void amominud(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amominud, rd, addr.base(), rs2, order);
	}
	void amomaxud(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed)
	{
		instructions_.emplace_back(InstructionType::RdRs2AddrOrder, &Assembler::amomaxud, rd, addr.base(), rs2, order);
	}
#endif // XLEN >= 64

#if XLEN == 32
	void lr(RegisterOperand rd, AddressOperand addr, std::memory_order order = std::memory_order_relaxed) { lrw(rd, addr, order); }
	void sr(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed) { scw(rd, rs2, addr, order); }
#elif XLEN == 64
	void lr(RegisterOperand rd, AddressOperand addr, std::memory_order order = std::memory_order_relaxed) { lrd(rd, addr, order); }
	void sr(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed) { scd(rd, rs2, addr, order); }
#elif XLEN == 128
	void lr(RegisterOperand rd, AddressOperand addr, std::memory_order order = std::memory_order_relaxed) { lrq(rd, addr, order); }
	void sr(RegisterOperand rd, RegisterOperand rs2, AddressOperand addr, std::memory_order order = std::memory_order_relaxed) { scq(rd, rs2, addr, order); }
#endif
	/*

		// Unsupported for now

		// ==== RV32F ====
		void flw(FRegisterOperand rd, AddressOperand addr);
		void fsw(FRegisterOperand rs2, AddressOperand addr);
		// rd := (rs1 * rs2) + rs3
		void fmadds(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, FRegisterOperand rs3, RoundingMode rounding = RNE);
		// rd := (rs1 * rs2) - rs3
		void fmsubs(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, FRegisterOperand rs3, RoundingMode rounding = RNE);
		// rd := -(rs1 * rs2) + rs3
		void fnmsubs(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, FRegisterOperand rs3, RoundingMode rounding = RNE);
		// rd := -(rs1 * rs2) - rs3
		void fnmadds(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, FRegisterOperand rs3, RoundingMode rounding = RNE);
		void fadds(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, RoundingMode rounding = RNE);
		void fsubs(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, RoundingMode rounding = RNE);
		void fmuls(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, RoundingMode rounding = RNE);
		void fdivs(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, RoundingMode rounding = RNE);
		void fsqrts(FRegisterOperand rd, FRegisterOperand rs1, RoundingMode rounding = RNE);
		void fsgnjs(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void fsgnjns(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void fsgnjxs(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void fmins(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void fmaxs(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void feqs(RegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void flts(RegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void fles(RegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void fclasss(RegisterOperand rd, FRegisterOperand rs1);
		// int32_t <- float
		void fcvtws(RegisterOperand rd, FRegisterOperand rs1, RoundingMode rounding = RNE);
		// uint32_t <- float
		void fcvtwus(RegisterOperand rd, FRegisterOperand rs1, RoundingMode rounding = RNE);
		// float <- int32_t
		void fcvtsw(FRegisterOperand rd, RegisterOperand rs1, RoundingMode rounding = RNE);
		// float <- uint32_t
		void fcvtswu(FRegisterOperand rd, RegisterOperand rs1, RoundingMode rounding = RNE);

		void fmvs(FRegisterOperand rd, FRegisterOperand rs) { fsgnjs(rd, rs, rs); }
		void fabss(FRegisterOperand rd, FRegisterOperand rs) { fsgnjxs(rd, rs, rs); }
		void fnegs(FRegisterOperand rd, FRegisterOperand rs) { fsgnjns(rd, rs, rs); }

		// xlen <--bit_cast-- float
		void fmvxw(RegisterOperand rd, FRegisterOperand rs1);
		// float <--bit_cast-- xlen
		void fmvwx(FRegisterOperand rd, RegisterOperand rs1);

	// ==== RV64F ====
	#if XLEN >= 64
		// int64_t <- double
		void fcvtls(RegisterOperand rd, FRegisterOperand rs1, RoundingMode rounding = RNE);
		// uint64_t <- double
		void fcvtlus(RegisterOperand rd, FRegisterOperand rs1, RoundingMode rounding = RNE);
		// double <- int64_t
		void fcvtsl(FRegisterOperand rd, RegisterOperand rs1, RoundingMode rounding = RNE);
		// double <- uint64_t
		void fcvtslu(FRegisterOperand rd, RegisterOperand rs1, RoundingMode rounding = RNE);
	#endif // XLEN >= 64

		// ==== RV32D ====
		void fld(FRegisterOperand rd, AddressOperand addr);
		void fsd(FRegisterOperand rs2, AddressOperand addr);
		// rd := (rs1 * rs2) + rs3
		void fmaddd(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, FRegisterOperand rs3, RoundingMode rounding = RNE);
		// rd := (rs1 * rs2) - rs3
		void fmsubd(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, FRegisterOperand rs3, RoundingMode rounding = RNE);
		// rd := -(rs1 * rs2) - rs3
		void fnmsubd(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, FRegisterOperand rs3, RoundingMode rounding = RNE);
		// rd := -(rs1 * rs2) + rs3
		void fnmaddd(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, FRegisterOperand rs3, RoundingMode rounding = RNE);
		void faddd(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, RoundingMode rounding = RNE);
		void fsubd(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, RoundingMode rounding = RNE);
		void fmuld(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, RoundingMode rounding = RNE);
		void fdivd(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2, RoundingMode rounding = RNE);
		void fsqrtd(FRegisterOperand rd, FRegisterOperand rs1, RoundingMode rounding = RNE);
		void fsgnjd(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void fsgnjnd(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void fsgnjxd(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void fmind(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void fmaxd(FRegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void fcvtsd(FRegisterOperand rd, FRegisterOperand rs1, RoundingMode rounding = RNE);
		void fcvtds(FRegisterOperand rd, FRegisterOperand rs1, RoundingMode rounding = RNE);
		void feqd(RegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void fltd(RegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void fled(RegisterOperand rd, FRegisterOperand rs1, FRegisterOperand rs2);
		void fclassd(RegisterOperand rd, FRegisterOperand rs1);
		// int32_t <- double
		void fcvtwd(RegisterOperand rd, FRegisterOperand rs1, RoundingMode rounding = RNE);
		// uint32_t <- double
		void fcvtwud(RegisterOperand rd, FRegisterOperand rs1, RoundingMode rounding = RNE);
		// double <- int32_t
		void fcvtdw(FRegisterOperand rd, RegisterOperand rs1, RoundingMode rounding = RNE);
		// double <- uint32_t
		void fcvtdwu(FRegisterOperand rd, RegisterOperand rs1, RoundingMode rounding = RNE);

		void fmvd(FRegisterOperand rd, FRegisterOperand rs) { fsgnjd(rd, rs, rs); }
		void fabsd(FRegisterOperand rd, FRegisterOperand rs) { fsgnjxd(rd, rs, rs); }
		void fnegd(FRegisterOperand rd, FRegisterOperand rs) { fsgnjnd(rd, rs, rs); }

	// ==== RV64D ====
	#if XLEN >= 64
		// int64_t <- double
		void fcvtld(RegisterOperand rd, FRegisterOperand rs1, RoundingMode rounding = RNE);
		// uint64_t <- double
		void fcvtlud(RegisterOperand rd, FRegisterOperand rs1, RoundingMode rounding = RNE);
		// xlen <--bit_cast-- double
		void fmvxd(RegisterOperand rd, FRegisterOperand rs1);
		// double <- int64_t
		void fcvtdl(FRegisterOperand rd, RegisterOperand rs1, RoundingMode rounding = RNE);
		// double <- uint64_t
		void fcvtdlu(FRegisterOperand rd, RegisterOperand rs1, RoundingMode rounding = RNE);
		// double <--bit_cast-- xlen
		void fmvdx(FRegisterOperand rd, RegisterOperand rs1);
	#endif // XLEN >= 64

		// ==== Zba: AddressOperand generation ====
		void adduw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void sh1add(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void sh1adduw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void sh2add(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void sh2adduw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void sh3add(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void sh3adduw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void slliuw(RegisterOperand rd, RegisterOperand rs1, intx_t imm);

		// ==== Zbb: Basic bit-manipulation ====
		void andn(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void orn(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void xnor(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void clz(RegisterOperand rd, RegisterOperand rs);
		void clzw(RegisterOperand rd, RegisterOperand rs);
		void ctz(RegisterOperand rd, RegisterOperand rs);
		void ctzw(RegisterOperand rd, RegisterOperand rs);
		void cpop(RegisterOperand rd, RegisterOperand rs);
		void cpopw(RegisterOperand rd, RegisterOperand rs);
		void max(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void maxu(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void min(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void minu(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void sextb(RegisterOperand rd, RegisterOperand rs);
		void sexth(RegisterOperand rd, RegisterOperand rs);
		void zexth(RegisterOperand rd, RegisterOperand rs);
		void rol(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void rolw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void ror(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void rori(RegisterOperand rd, RegisterOperand rs1, intx_t imm);
		void roriw(RegisterOperand rd, RegisterOperand rs1, intx_t imm);
		void rorw(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void orcb(RegisterOperand rd, RegisterOperand rs);
		void rev8(RegisterOperand rd, RegisterOperand rs);

		// ==== Zbc: Carry-less multiplication ====
		void clmul(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void clmulh(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void clmulr(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);

		// ==== Zbs: Single-bit instructions ====
		void bclr(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void bclri(RegisterOperand rd, RegisterOperand rs1, intx_t shamt);
		void bext(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void bexti(RegisterOperand rd, RegisterOperand rs1, intx_t shamt);
		void binv(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void binvi(RegisterOperand rd, RegisterOperand rs1, intx_t shamt);
		void bset(RegisterOperand rd, RegisterOperand rs1, RegisterOperand rs2);
		void bseti(RegisterOperand rd, RegisterOperand rs1, intx_t shamt);

	*/

	RegisterOperand next_register() { return RegisterOperand { reg_allocator_index_++ }; }

	void reset()
	{
		reg_allocator_index_ = RegisterOperand::VIRTUAL_BASE;
		instructions_.clear();
		labels_.clear();
	}

	Label *alloc_label() { return labels_.emplace_back(std::make_unique<Label>()).get(); }

	void allocate()
	{
		// reverse linear scan allocator
#ifdef DEBUG_REGALLOC
		DEBUG_STREAM << "REGISTER ALLOCATION" << std::endl;
#endif
		// TODO: handle different sizes

		std::unordered_map<unsigned int, unsigned int> vreg_to_preg;

		// All registers can be used except:
		// SP (x2),
		// FP/RegFile Ptr (x8),
		// zero (x0)
		// Return to trampoline (x1)
		// Memory base (x31),
		// thread pointer (x4)
		// Return value (x10 + x11) can be used once first def

		std::bitset<32> avail_physregs = 0x7FFFF2E8;
		//		std::bitset<32> avail_float_physregs = 0xFFFFFFFFF;

		for (auto RI = instructions_.rbegin(), RE = instructions_.rend(); RI != RE; RI++) {
			auto &insn = *RI;

#ifdef DEBUG_REGALLOC
			DEBUG_STREAM << "considering instruction ";
			insn.dump(DEBUG_STREAM);
			DEBUG_STREAM << '\n';
#endif

			auto allocate = [&avail_physregs /*, &avail_float_physregs*/, &vreg_to_preg](RegisterOperand *o, RegisterOperand preference) -> void {
				unsigned int vri;
				//				ir::value_type type;
				if (!o->is_physical()) {
					vri = o->encoding();
					//					type = o.vreg().type();
				} else {
					throw std::runtime_error("Trying to allocate non-virtual register operand");
				}

				size_t allocation = 0;
				//				if (type.is_floating_point()) {
				//					allocation = avail_float_physregs._Find_first();
				//					avail_float_physregs.flip(allocation);
				//				} else {
				if (preference && avail_physregs.test(preference.encoding())) {
					allocation = preference.encoding(); // Prefer reusing the destination register
				} else {
					allocation = avail_physregs._Find_next(7); // Try using the C registers (x8-x15 first)
					if (allocation == 32) {
						allocation = avail_physregs._Find_first();
					}
				}
				avail_physregs.flip(allocation);
				//				}

				// TODO: register spilling
				vreg_to_preg[vri] = allocation;

				o->allocate(allocation /*, type*/);
			};

			// kill rd def first
			if (insn.rd) {
				if (!insn.rd.is_physical()) {
#ifdef DEBUG_REGALLOC
					DEBUG_STREAM << "  DEF ";
					o.dump(DEBUG_STREAM);
#endif

					//					auto type = o.vreg().type();
					unsigned int vri = insn.rd.encoding();

					auto alloc = vreg_to_preg.find(vri);

					if (alloc != vreg_to_preg.end()) {
						int pri = alloc->second;
						if (!insn.is_rd_use() && !insn.rd.is_functional()) { // Those stay active
							//							if (type.is_floating_point()) {
							//								avail_float_physregs.set(pri);
							//							} else {
							avail_physregs.set(pri);
							vreg_to_preg.erase(alloc); // Clear allocations, so unrelated earlier usages of the same vreg don't interfere
							//							}
						}
						insn.rd.allocate(pri /*, type*/);
#ifdef DEBUG_REGALLOC
						DEBUG_STREAM << " allocated to ";
						o.dump(DEBUG_STREAM);
						DEBUG_STREAM << " -- releasing\n";
#endif
					} else {
#ifdef DEBUG_REGALLOC
						DEBUG_STREAM << " not allocated - killing instruction" << std::endl;
#endif
						insn.kill();
					}

#ifdef DEBUG_REGALLOC
					DEBUG_STREAM << std::endl;
#endif
				} else {
					if ((insn.rd == A0 || insn.rd == A1) && !insn.is_rd_use()) {
						// Assigning to ret value, so free the register
						avail_physregs.set(insn.rd.encoding());
					}
				}
			}

			if (insn.is_dead()) {
				continue;
			}

			// alloc uses next

			RegisterOperand *ops[] { &insn.rs1, &insn.rs2 };

			for (auto &o : ops) {
				if (*o && !o->is_physical()) {
#ifdef DEBUG_REGALLOC
					DEBUG_STREAM << "  USE ";
					o.dump(DEBUG_STREAM);
					DEBUG_STREAM << '\n';
#endif

					//					auto type = o.vreg().type();
					unsigned int vri = o->encoding();
					if (!vreg_to_preg.count(vri)) {
						allocate(o, insn.rd);
#ifdef DEBUG_REGALLOC
						DEBUG_STREAM << " allocating vreg to ";
						o.dump(DEBUG_STREAM);
						DEBUG_STREAM << '\n';
#endif
					} else {
						o->allocate(vreg_to_preg.at(vri) /*, type*/);
					}

#ifdef DEBUG_REGALLOC
					DEBUG_STREAM << std::endl;
#endif
				}
			}

			// Kill MOVs
			// TODO: refactor
			if (insn.is_mv()) {
				RegisterOperand op1 = insn.rd;
				RegisterOperand op2 = insn.rs1;

				if (op1.is_physical() && op2.is_physical()) {
					if (op1.encoding() == op2.encoding()) {
						insn.kill();
					}
				}
			}
		}
	}

	void emit(Assembler &assembler)
	{
		for (const auto &item : instructions_) {
			item.emit(assembler);
		}
	}

private:
	uint32_t reg_allocator_index_ { RegisterOperand::VIRTUAL_BASE };
	std::vector<Instruction> instructions_;
	std::vector<std::unique_ptr<Label>> labels_;
};
} // namespace arancini::output::dynamic::riscv64::builder