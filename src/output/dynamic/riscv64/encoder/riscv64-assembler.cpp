// Copyright (c) 2017, the Newspeak project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include <arancini/output/dynamic/riscv64/encoder/riscv64-assembler.h>

#include <cstdlib>


namespace arancini::output::dynamic::riscv64 {

Assembler::Assembler(arancini::output::dynamic::machine_code_writer *writer, bool track_usage, ExtensionSet extensions)
	: extensions_(extensions)
	, writer { writer }
	, track_usage_ { track_usage }
{
}

Assembler::~Assembler(){
	if (track_usage_) {
		std::cout << "Total RISC-V instructions emitted: " << std::dec << instructions32 << " full size, " << instructions16 << " compressed" << std::endl;
	}
}

void Assembler::Bind(Label *label)
{
	ASSERT(!label->IsBound());
	intptr_t target_position = writer->size();

	while (label->IsLinked()) {
		int32_t branch_position = label->Position();
		ASSERT(Utils::IsAligned(branch_position, 2));
		int32_t offset = target_position - branch_position;
		ASSERT(Utils::IsAligned(offset, 2));

		if (IsCInstruction(Read16(branch_position))) {
			uint16_t old_branch = Read16(branch_position);
			uint16_t new_branch = EncodeCBranchOrJumpOffset(offset, old_branch);
			Write16(branch_position, new_branch);
			label->position_ = DecodeCBranchOrJumpOffset(old_branch);
		} else {
			uint32_t old_branch = Read32(branch_position);
			uint32_t new_branch = EncodeBranchOrJumpOffset(offset, old_branch);
			Write32(branch_position, new_branch);
			label->position_ = DecodeBranchOrJumpOffset(old_branch);
		}
	}

	label->BindTo(target_position);
}

uint32_t Assembler::EncodeBranchOrJumpOffset(int32_t offset, uint32_t encoded)
{
	ASSERT(offset != 0);
	Instruction instr(encoded);
	if (instr.opcode() == BRANCH) {
		uint32_t e = 0;
		e |= EncodeRs2(instr.rs2());
		e |= EncodeRs1(instr.rs1());
		e |= EncodeFunct3(instr.funct3());
		e |= EncodeOpcode(instr.opcode());
		e |= EncodeBTypeImm(offset);
		return e;
	} else if (instr.opcode() == JAL) {
		uint32_t e = 0;
		e |= EncodeRd(instr.rd());
		e |= EncodeOpcode(instr.opcode());
		e |= EncodeJTypeImm(offset);
		return e;
	} else {
		UNREACHABLE();
		return 0;
	}
}

int32_t Assembler::DecodeBranchOrJumpOffset(uint32_t encoded)
{
	Instruction instr(encoded);
	if (instr.opcode() == BRANCH) {
		return instr.btype_imm();
	} else if (instr.opcode() == JAL) {
		return instr.jtype_imm();
	} else {
		UNREACHABLE();
		return 0;
	}
}

uint32_t Assembler::EncodeCBranchOrJumpOffset(int32_t offset, uint32_t encoded)
{
	ASSERT(offset != 0);
	CInstruction instr(encoded);
	if ((instr.opcode() == C_BEQZ) || (instr.opcode() == C_BNEZ)) {
		return instr.opcode() | EncodeCRs1p(instr.rs1p()) | EncodeCBImm(offset);
	} else if ((instr.opcode() == C_J) || (instr.opcode() == C_JAL)) {
		return instr.opcode() | EncodeCJImm(offset);
	}
	UNREACHABLE();
	return 0;
}

int32_t Assembler::DecodeCBranchOrJumpOffset(uint32_t encoded)
{
	CInstruction instr(encoded);
	if ((instr.opcode() == C_BEQZ) || (instr.opcode() == C_BNEZ)) {
		return instr.b_imm();
	} else if ((instr.opcode() == C_J) || (instr.opcode() == C_JAL)) {
		return instr.j_imm();
	}
	UNREACHABLE();
	return 0;
}

void Assembler::lui(Register rd, intptr_t imm)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C) && (rd != ZERO) && (rd != SP) && IsCUImm(imm)) {
		c_lui(rd, imm);
		return;
	}
	EmitUType(imm, rd, LUI);
}

void Assembler::auipc(Register rd, intptr_t imm)
{
	ASSERT(Supports(RV_I));
	EmitUType(imm, rd, AUIPC);
}

void Assembler::jal(Register rd, Label *label, bool near)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C) && near) {
		if (rd == ZERO) {
			c_j(label);
			return;
		}
#if XLEN == 32
		if (rd == RA) {
			c_jal(label);
			return;
		}
#endif // XLEN == 32
	}
	EmitJump(rd, label, JAL);
}

intptr_t Assembler::offset_from_target(intptr_t target) { return target - (reinterpret_cast<uword>(writer->ptr()) + Position()); }

void Assembler::jal(Register rd, intptr_t offset)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C) && IsCJImm(offset)) {
		if (rd == ZERO) {
			c_j(offset);
			return;
		}
	}
	EmitJump(rd, offset, JAL);
}

void Assembler::jalr(Register rd, Register rs1, intptr_t offset)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if (rs1 != ZERO && offset == 0) {
			if (rd == ZERO) {
				c_jr(rs1);
				return;
			} else if (rd == RA) {
				c_jalr(rs1);
				return;
			}
		}
	}
	EmitIType(offset, rs1, F3_0, rd, JALR);
}

void Assembler::beq(Register rs1, Register rs2, Label *label, bool near)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C) && near) {
		if (rs1 == ZERO && IsCRs1p(rs2)) {
			c_beqz(rs2, label);
			return;
		} else if (rs2 == ZERO && IsCRs1p(rs1)) {
			c_beqz(rs1, label);
			return;
		}
	}
	EmitBranch(rs1, rs2, label, BEQ);
}

void Assembler::beq(Register rs1, Register rs2, intptr_t offset)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C) && IsCBImm(offset)) {
		if (rs1 == ZERO && IsCRs1p(rs2)) {
			c_beqz(rs2, offset);
			return;
		} else if (rs2 == ZERO && IsCRs1p(rs1)) {
			c_beqz(rs1, offset);
			return;
		}
	}
	EmitBranch(rs1, rs2, offset, BEQ);
}

void Assembler::bne(Register rs1, Register rs2, Label *label, bool near)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C) && near) {
		if (rs1 == ZERO && IsCRs1p(rs2)) {
			c_bnez(rs2, label);
			return;
		} else if (rs2 == ZERO && IsCRs1p(rs1)) {
			c_bnez(rs1, label);
			return;
		}
	}
	EmitBranch(rs1, rs2, label, BNE);
}

void Assembler::bne(Register rs1, Register rs2, intptr_t offset)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C) && IsCBImm(offset)) {
		if (rs1 == ZERO && IsCRs1p(rs2)) {
			c_bnez(rs2, offset);
			return;
		} else if (rs2 == ZERO && IsCRs1p(rs1)) {
			c_bnez(rs1, offset);
			return;
		}
	}
	EmitBranch(rs1, rs2, offset, BNE);
}

void Assembler::blt(Register rs1, Register rs2, Label *label)
{
	ASSERT(Supports(RV_I));
	EmitBranch(rs1, rs2, label, BLT);
}

void Assembler::blt(Register rs1, Register rs2, intptr_t offset)
{
	ASSERT(Supports(RV_I));
	EmitBranch(rs1, rs2, offset, BLT);
}

void Assembler::bge(Register rs1, Register rs2, Label *label)
{
	ASSERT(Supports(RV_I));
	EmitBranch(rs1, rs2, label, BGE);
}

void Assembler::bge(Register rs1, Register rs2, intptr_t offset)
{
	ASSERT(Supports(RV_I));
	EmitBranch(rs1, rs2, offset, BGE);
}

void Assembler::bltu(Register rs1, Register rs2, Label *label)
{
	ASSERT(Supports(RV_I));
	EmitBranch(rs1, rs2, label, BLTU);
}

void Assembler::bltu(Register rs1, Register rs2, intptr_t offset)
{
	ASSERT(Supports(RV_I));
	EmitBranch(rs1, rs2, offset, BLTU);
}

void Assembler::bgeu(Register rs1, Register rs2, Label *label) { EmitBranch(rs1, rs2, label, BGEU); }

void Assembler::bgeu(Register rs1, Register rs2, intptr_t offset) { EmitBranch(rs1, rs2, offset, BGEU); }

void Assembler::lb(Register rd, Address addr)
{
	ASSERT(Supports(RV_I));
	EmitIType(addr.offset(), addr.base(), LB, rd, LOAD);
}

void Assembler::lh(Register rd, Address addr)
{
	ASSERT(Supports(RV_I));
	EmitIType(addr.offset(), addr.base(), LH, rd, LOAD);
}

void Assembler::lw(Register rd, Address addr)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((addr.base() == SP) && IsCSPLoad4Imm(addr.offset())) {
			c_lwsp(rd, addr);
			return;
		}
		if (IsCRdp(rd) && IsCRs1p(addr.base()) && IsCMem4Imm(addr.offset())) {
			c_lw(rd, addr);
			return;
		}
	}
	EmitIType(addr.offset(), addr.base(), LW, rd, LOAD);
}

void Assembler::lbu(Register rd, Address addr)
{
	ASSERT(Supports(RV_I));
	EmitIType(addr.offset(), addr.base(), LBU, rd, LOAD);
}

void Assembler::lhu(Register rd, Address addr)
{
	ASSERT(Supports(RV_I));
	EmitIType(addr.offset(), addr.base(), LHU, rd, LOAD);
}

void Assembler::sb(Register rs2, Address addr)
{
	ASSERT(Supports(RV_I));
	EmitSType(addr.offset(), rs2, addr.base(), SB, STORE);
}

void Assembler::sh(Register rs2, Address addr)
{
	ASSERT(Supports(RV_I));
	EmitSType(addr.offset(), rs2, addr.base(), SH, STORE);
}

void Assembler::sw(Register rs2, Address addr)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((addr.base() == SP) && IsCSPStore4Imm(addr.offset())) {
			c_swsp(rs2, addr);
			return;
		}
		if (IsCRs2p(rs2) && IsCRs1p(addr.base()) && IsCMem4Imm(addr.offset())) {
			c_sw(rs2, addr);
			return;
		}
	}
	EmitSType(addr.offset(), rs2, addr.base(), SW, STORE);
}

void Assembler::addi(Register rd, Register rs1, intptr_t imm, bool force_big)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C) && !force_big) {
		if ((rd != ZERO) && (rs1 == ZERO) && IsCIImm(imm)) {
			c_li(rd, imm);
			return;
		}
		if ((rd == rs1) && IsCIImm(imm) && (imm != 0)) {
			c_addi(rd, rs1, imm);
			return;
		}
		if ((rd == SP) && (rs1 == SP) && IsCI16Imm(imm) && (imm != 0)) {
			c_addi16sp(rd, rs1, imm);
			return;
		}
		if (IsCRdp(rd) && (rs1 == SP) && IsCI4SPNImm(imm) && (imm != 0)) {
			c_addi4spn(rd, rs1, imm);
			return;
		}
		if (imm == 0) {
			if ((rd == ZERO) && (rs1 == ZERO)) {
				c_nop();
				return;
			}
			if ((rd != ZERO) && (rs1 != ZERO)) {
				c_mv(rd, rs1);
				return;
			}
		}
	}
	EmitIType(imm, rs1, ADDI, rd, OPIMM);
}

void Assembler::slti(Register rd, Register rs1, intptr_t imm)
{
	ASSERT(Supports(RV_I));
	EmitIType(imm, rs1, SLTI, rd, OPIMM);
}

void Assembler::sltiu(Register rd, Register rs1, intptr_t imm)
{
	ASSERT(Supports(RV_I));
	EmitIType(imm, rs1, SLTIU, rd, OPIMM);
}

void Assembler::xori(Register rd, Register rs1, intptr_t imm)
{
	ASSERT(Supports(RV_I));
	EmitIType(imm, rs1, XORI, rd, OPIMM);
}

void Assembler::ori(Register rd, Register rs1, intptr_t imm)
{
	ASSERT(Supports(RV_I));
	EmitIType(imm, rs1, ORI, rd, OPIMM);
}

void Assembler::andi(Register rd, Register rs1, intptr_t imm)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((rd == rs1) && IsCRs1p(rs1) && IsCIImm(imm)) {
			c_andi(rd, rs1, imm);
			return;
		}
	}
	EmitIType(imm, rs1, ANDI, rd, OPIMM);
}

void Assembler::slli(Register rd, Register rs1, intptr_t shamt)
{
	ASSERT((shamt > 0) && (shamt < XLEN));
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((rd == rs1) && (shamt != 0)) {
			c_slli(rd, rs1, shamt);
			return;
		}
	}
	EmitRType(F7_0, shamt, rs1, SLLI, rd, OPIMM);
}

void Assembler::srli(Register rd, Register rs1, intptr_t shamt)
{
	ASSERT((shamt > 0) && (shamt < XLEN));
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((rd == rs1) && IsCRs1p(rs1) && (shamt != 0)) {
			c_srli(rd, rs1, shamt);
			return;
		}
	}
	EmitRType(F7_0, shamt, rs1, SRI, rd, OPIMM);
}

void Assembler::srai(Register rd, Register rs1, intptr_t shamt)
{
	ASSERT((shamt > 0) && (shamt < XLEN));
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((rd == rs1) && IsCRs1p(rs1) && (shamt != 0)) {
			c_srai(rd, rs1, shamt);
			return;
		}
	}
	EmitRType(SRA, shamt, rs1, SRI, rd, OPIMM);
}

void Assembler::add(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if (rd == rs1) {
			c_add(rd, rs1, rs2);
			return;
		}
		if (rd == rs2) {
			c_add(rd, rs2, rs1);
			return;
		}
	}
	EmitRType(F7_0, rs2, rs1, ADD, rd, OP);
}

void Assembler::sub(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((rd == rs1) && IsCRs1p(rs1) && IsCRs2p(rs2)) {
			c_sub(rd, rs1, rs2);
			return;
		}
	}
	EmitRType(SUB, rs2, rs1, ADD, rd, OP);
}

void Assembler::sll(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	EmitRType(F7_0, rs2, rs1, SLL, rd, OP);
}

void Assembler::slt(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	EmitRType(F7_0, rs2, rs1, SLT, rd, OP);
}

void Assembler::sltu(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	EmitRType(F7_0, rs2, rs1, SLTU, rd, OP);
}

void Assembler::xor_(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((rd == rs1) && IsCRs1p(rs1) && IsCRs2p(rs2)) {
			c_xor(rd, rs1, rs2);
			return;
		}
		if ((rd == rs2) && IsCRs1p(rs1) && IsCRs2p(rs2)) {
			c_xor(rd, rs2, rs1);
			return;
		}
	}
	EmitRType(F7_0, rs2, rs1, XOR, rd, OP);
}

void Assembler::srl(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	EmitRType(F7_0, rs2, rs1, SR, rd, OP);
}

void Assembler::sra(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	EmitRType(SRA, rs2, rs1, SR, rd, OP);
}

void Assembler::or_(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((rd == rs1) && IsCRs1p(rs1) && IsCRs2p(rs2)) {
			c_or(rd, rs1, rs2);
			return;
		}
		if ((rd == rs2) && IsCRs1p(rs1) && IsCRs2p(rs2)) {
			c_or(rd, rs2, rs1);
			return;
		}
	}
	EmitRType(F7_0, rs2, rs1, OR, rd, OP);
}

void Assembler::and_(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((rd == rs1) && IsCRs1p(rs1) && IsCRs2p(rs2)) {
			c_and(rd, rs1, rs2);
			return;
		}
		if ((rd == rs2) && IsCRs1p(rs1) && IsCRs2p(rs2)) {
			c_and(rd, rs2, rs1);
			return;
		}
	}
	EmitRType(F7_0, rs2, rs1, AND, rd, OP);
}

void Assembler::fence(HartEffects predecessor, HartEffects successor)
{
	ASSERT((predecessor & kAll) == predecessor);
	ASSERT((successor & kAll) == successor);
	ASSERT(Supports(RV_I));
	EmitIType((predecessor << 4) | successor, ZERO, FENCE, ZERO, MISCMEM);
}

void Assembler::fencei()
{
	ASSERT(Supports(RV_I));
	EmitIType(0, ZERO, FENCEI, ZERO, MISCMEM);
}

void Assembler::ecall()
{
	ASSERT(Supports(RV_I));
	EmitIType(ECALL, ZERO, F3_0, ZERO, SYSTEM);
}
void Assembler::ebreak()
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		c_ebreak();
		return;
	}
	EmitIType(EBREAK, ZERO, F3_0, ZERO, SYSTEM);
}

void Assembler::csrrw(Register rd, uint32_t csr, Register rs1)
{
	ASSERT(Supports(RV_I));
	EmitIType(csr, rs1, CSRRW, rd, SYSTEM);
}

void Assembler::csrrs(Register rd, uint32_t csr, Register rs1)
{
	ASSERT(Supports(RV_I));
	EmitIType(csr, rs1, CSRRS, rd, SYSTEM);
}

void Assembler::csrrc(Register rd, uint32_t csr, Register rs1)
{
	ASSERT(Supports(RV_I));
	EmitIType(csr, rs1, CSRRC, rd, SYSTEM);
}

void Assembler::csrrwi(Register rd, uint32_t csr, uint32_t imm)
{
	ASSERT(Supports(RV_I));
	EmitIType(csr, Register(imm), CSRRWI, rd, SYSTEM);
}

void Assembler::csrrsi(Register rd, uint32_t csr, uint32_t imm)
{
	ASSERT(Supports(RV_I));
	EmitIType(csr, Register(imm), CSRRSI, rd, SYSTEM);
}

void Assembler::csrrci(Register rd, uint32_t csr, uint32_t imm)
{
	ASSERT(Supports(RV_I));
	EmitIType(csr, Register(imm), CSRRCI, rd, SYSTEM);
}

void Assembler::trap()
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		Emit16(0); // Permanently reserved illegal instruction.
	} else {
		Emit32(0); // Permanently reserved illegal instruction.
	}
}

#if XLEN >= 64
void Assembler::lwu(Register rd, Address addr)
{
	ASSERT(Supports(RV_I));
	EmitIType(addr.offset(), addr.base(), LWU, rd, LOAD);
}

void Assembler::ld(Register rd, Address addr)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((addr.base() == SP) && IsCSPLoad8Imm(addr.offset())) {
			c_ldsp(rd, addr);
			return;
		}
		if (IsCRdp(rd) && IsCRs1p(addr.base()) && IsCMem8Imm(addr.offset())) {
			c_ld(rd, addr);
			return;
		}
	}
	EmitIType(addr.offset(), addr.base(), LD, rd, LOAD);
}

void Assembler::sd(Register rs2, Address addr)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((addr.base() == SP) && IsCSPStore8Imm(addr.offset())) {
			c_sdsp(rs2, addr);
			return;
		}
		if (IsCRs2p(rs2) && IsCRs1p(addr.base()) && IsCMem8Imm(addr.offset())) {
			c_sd(rs2, addr);
			return;
		}
	}
	EmitSType(addr.offset(), rs2, addr.base(), SD, STORE);
}

void Assembler::addiw(Register rd, Register rs1, intptr_t imm)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((rd != ZERO) && (rs1 == ZERO) && IsCIImm(imm)) {
			c_li(rd, imm);
			return;
		}
		if ((rd == rs1) && (rd != ZERO) && IsCIImm(imm)) {
			c_addiw(rd, rs1, imm);
			return;
		}
	}
	EmitIType(imm, rs1, ADDI, rd, OPIMM32);
}

void Assembler::slliw(Register rd, Register rs1, intptr_t shamt)
{
	ASSERT((shamt > 0) && (shamt < 32));
	ASSERT(Supports(RV_I));
	EmitRType(F7_0, shamt, rs1, SLLI, rd, OPIMM32);
}

void Assembler::srliw(Register rd, Register rs1, intptr_t shamt)
{
	ASSERT((shamt > 0) && (shamt < 32));
	ASSERT(Supports(RV_I));
	EmitRType(F7_0, shamt, rs1, SRI, rd, OPIMM32);
}

void Assembler::sraiw(Register rd, Register rs1, intptr_t shamt)
{
	ASSERT((shamt > 0) && (shamt < XLEN));
	ASSERT(Supports(RV_I));
	EmitRType(SRA, shamt, rs1, SRI, rd, OPIMM32);
}

void Assembler::addw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((rd == rs1) && IsCRs1p(rs1) && IsCRs2p(rs2)) {
			c_addw(rd, rs1, rs2);
			return;
		}
		if ((rd == rs2) && IsCRs1p(rs1) && IsCRs2p(rs2)) {
			c_addw(rd, rs2, rs1);
			return;
		}
	}
	EmitRType(F7_0, rs2, rs1, ADD, rd, OP32);
}

void Assembler::subw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	if (Supports(RV_C)) {
		if ((rd == rs1) && IsCRs1p(rs1) && IsCRs2p(rs2)) {
			c_subw(rd, rs1, rs2);
			return;
		}
	}
	EmitRType(SUB, rs2, rs1, ADD, rd, OP32);
}

void Assembler::sllw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	EmitRType(F7_0, rs2, rs1, SLL, rd, OP32);
}

void Assembler::srlw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	EmitRType(F7_0, rs2, rs1, SR, rd, OP32);
}
void Assembler::sraw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_I));
	EmitRType(SRA, rs2, rs1, SR, rd, OP32);
}
#endif // XLEN >= 64

void Assembler::mul(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_M));
	EmitRType(MULDIV, rs2, rs1, MUL, rd, OP);
}

void Assembler::mulh(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_M));
	EmitRType(MULDIV, rs2, rs1, MULH, rd, OP);
}

void Assembler::mulhsu(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_M));
	EmitRType(MULDIV, rs2, rs1, MULHSU, rd, OP);
}

void Assembler::mulhu(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_M));
	EmitRType(MULDIV, rs2, rs1, MULHU, rd, OP);
}

void Assembler::div(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_M));
	EmitRType(MULDIV, rs2, rs1, DIV, rd, OP);
}

void Assembler::divu(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_M));
	EmitRType(MULDIV, rs2, rs1, DIVU, rd, OP);
}

void Assembler::rem(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_M));
	EmitRType(MULDIV, rs2, rs1, REM, rd, OP);
}

void Assembler::remu(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_M));
	EmitRType(MULDIV, rs2, rs1, REMU, rd, OP);
}

#if XLEN >= 64
void Assembler::mulw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_M));
	EmitRType(MULDIV, rs2, rs1, MULW, rd, OP32);
}

void Assembler::divw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_M));
	EmitRType(MULDIV, rs2, rs1, DIVW, rd, OP32);
}

void Assembler::divuw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_M));
	EmitRType(MULDIV, rs2, rs1, DIVUW, rd, OP32);
}

void Assembler::remw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_M));
	EmitRType(MULDIV, rs2, rs1, REMW, rd, OP32);
}

void Assembler::remuw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_M));
	EmitRType(MULDIV, rs2, rs1, REMUW, rd, OP32);
}
#endif // XLEN >= 64

void Assembler::lrw(Register rd, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(LR, order, ZERO, addr.base(), WIDTH32, rd, AMO);
}
void Assembler::scw(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(SC, order, rs2, addr.base(), WIDTH32, rd, AMO);
}

void Assembler::amoswapw(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOSWAP, order, rs2, addr.base(), WIDTH32, rd, AMO);
}

void Assembler::amoaddw(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOADD, order, rs2, addr.base(), WIDTH32, rd, AMO);
}

void Assembler::amoxorw(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOXOR, order, rs2, addr.base(), WIDTH32, rd, AMO);
}

void Assembler::amoandw(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOAND, order, rs2, addr.base(), WIDTH32, rd, AMO);
}

void Assembler::amoorw(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOOR, order, rs2, addr.base(), WIDTH32, rd, AMO);
}

void Assembler::amominw(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOMIN, order, rs2, addr.base(), WIDTH32, rd, AMO);
}

void Assembler::amomaxw(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOMAX, order, rs2, addr.base(), WIDTH32, rd, AMO);
}

void Assembler::amominuw(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOMINU, order, rs2, addr.base(), WIDTH32, rd, AMO);
}

void Assembler::amomaxuw(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOMAXU, order, rs2, addr.base(), WIDTH32, rd, AMO);
}

#if XLEN >= 64
void Assembler::lrd(Register rd, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(LR, order, ZERO, addr.base(), WIDTH64, rd, AMO);
}

void Assembler::scd(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(SC, order, rs2, addr.base(), WIDTH64, rd, AMO);
}

void Assembler::amoswapd(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOSWAP, order, rs2, addr.base(), WIDTH64, rd, AMO);
}

void Assembler::amoaddd(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOADD, order, rs2, addr.base(), WIDTH64, rd, AMO);
}

void Assembler::amoxord(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOXOR, order, rs2, addr.base(), WIDTH64, rd, AMO);
}

void Assembler::amoandd(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOAND, order, rs2, addr.base(), WIDTH64, rd, AMO);
}

void Assembler::amoord(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOOR, order, rs2, addr.base(), WIDTH64, rd, AMO);
}

void Assembler::amomind(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOMIN, order, rs2, addr.base(), WIDTH64, rd, AMO);
}

void Assembler::amomaxd(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOMAX, order, rs2, addr.base(), WIDTH64, rd, AMO);
}

void Assembler::amominud(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOMINU, order, rs2, addr.base(), WIDTH64, rd, AMO);
}

void Assembler::amomaxud(Register rd, Register rs2, Address addr, std::memory_order order)
{
	ASSERT(addr.offset() == 0);
	ASSERT(Supports(RV_A));
	EmitRType(AMOMAXU, order, rs2, addr.base(), WIDTH64, rd, AMO);
}
#endif // XLEN >= 64

void Assembler::flw(FRegister rd, Address addr)
{
	ASSERT(Supports(RV_F));
#if XLEN == 32
	if (Supports(RV_C)) {
		if ((addr.base() == SP) && IsCSPLoad4Imm(addr.offset())) {
			c_flwsp(rd, addr);
			return;
		}
		if (IsCFRdp(rd) && IsCRs1p(addr.base()) && IsCMem4Imm(addr.offset())) {
			c_flw(rd, addr);
			return;
		}
	}
#endif // XLEN == 32
	EmitIType(addr.offset(), addr.base(), S, rd, LOADFP);
}

void Assembler::fsw(FRegister rs2, Address addr)
{
	ASSERT(Supports(RV_F));
#if XLEN == 32
	if (Supports(RV_C)) {
		if ((addr.base() == SP) && IsCSPStore4Imm(addr.offset())) {
			c_fswsp(rs2, addr);
			return;
		}
		if (IsCFRs2p(rs2) && IsCRs1p(addr.base()) && IsCMem4Imm(addr.offset())) {
			c_fsw(rs2, addr);
			return;
		}
	}
#endif // XLEN == 32
	EmitSType(addr.offset(), rs2, addr.base(), S, STOREFP);
}

void Assembler::fmadds(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitR4Type(rs3, F2_S, rs2, rs1, rounding, rd, FMADD);
}

void Assembler::fmsubs(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitR4Type(rs3, F2_S, rs2, rs1, rounding, rd, FMSUB);
}

void Assembler::fnmsubs(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitR4Type(rs3, F2_S, rs2, rs1, rounding, rd, FNMSUB);
}

void Assembler::fnmadds(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitR4Type(rs3, F2_S, rs2, rs1, rounding, rd, FNMADD);
}

void Assembler::fadds(FRegister rd, FRegister rs1, FRegister rs2, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitRType(FADDS, rs2, rs1, rounding, rd, OPFP);
}

void Assembler::fsubs(FRegister rd, FRegister rs1, FRegister rs2, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitRType(FSUBS, rs2, rs1, rounding, rd, OPFP);
}

void Assembler::fmuls(FRegister rd, FRegister rs1, FRegister rs2, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitRType(FMULS, rs2, rs1, rounding, rd, OPFP);
}

void Assembler::fdivs(FRegister rd, FRegister rs1, FRegister rs2, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitRType(FDIVS, rs2, rs1, rounding, rd, OPFP);
}

void Assembler::fsqrts(FRegister rd, FRegister rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitRType(FSQRTS, FRegister(0), rs1, rounding, rd, OPFP);
}

void Assembler::fsgnjs(FRegister rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_F));
	EmitRType(FSGNJS, rs2, rs1, J, rd, OPFP);
}

void Assembler::fsgnjns(FRegister rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_F));
	EmitRType(FSGNJS, rs2, rs1, JN, rd, OPFP);
}

void Assembler::fsgnjxs(FRegister rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_F));
	EmitRType(FSGNJS, rs2, rs1, JX, rd, OPFP);
}

void Assembler::fmins(FRegister rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_F));
	EmitRType(FMINMAXS, rs2, rs1, FMIN, rd, OPFP);
}

void Assembler::fmaxs(FRegister rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_F));
	EmitRType(FMINMAXS, rs2, rs1, FMAX, rd, OPFP);
}

void Assembler::feqs(Register rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_F));
	EmitRType(FCMPS, rs2, rs1, FEQ, rd, OPFP);
}

void Assembler::flts(Register rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_F));
	EmitRType(FCMPS, rs2, rs1, FLT, rd, OPFP);
}

void Assembler::fles(Register rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_F));
	EmitRType(FCMPS, rs2, rs1, FLE, rd, OPFP);
}

void Assembler::fclasss(Register rd, FRegister rs1)
{
	ASSERT(Supports(RV_F));
	EmitRType(FCLASSS, FRegister(0), rs1, F3_1, rd, OPFP);
}

void Assembler::fcvtws(Register rd, FRegister rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitRType(FCVTintS, FRegister(W), rs1, rounding, rd, OPFP);
}

void Assembler::fcvtwus(Register rd, FRegister rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitRType(FCVTintS, FRegister(WU), rs1, rounding, rd, OPFP);
}

void Assembler::fcvtsw(FRegister rd, Register rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitRType(FCVTSint, FRegister(W), rs1, rounding, rd, OPFP);
}

void Assembler::fcvtswu(FRegister rd, Register rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitRType(FCVTSint, FRegister(WU), rs1, rounding, rd, OPFP);
}

void Assembler::fmvxw(Register rd, FRegister rs1)
{
	ASSERT(Supports(RV_F));
	EmitRType(FMVXW, FRegister(0), rs1, F3_0, rd, OPFP);
}

void Assembler::fmvwx(FRegister rd, Register rs1)
{
	ASSERT(Supports(RV_F));
	EmitRType(FMVWX, FRegister(0), rs1, F3_0, rd, OPFP);
}

#if XLEN >= 64
void Assembler::fcvtls(Register rd, FRegister rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitRType(FCVTintS, FRegister(L), rs1, rounding, rd, OPFP);
}

void Assembler::fcvtlus(Register rd, FRegister rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitRType(FCVTintS, FRegister(LU), rs1, rounding, rd, OPFP);
}

void Assembler::fcvtsl(FRegister rd, Register rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitRType(FCVTSint, FRegister(L), rs1, rounding, rd, OPFP);
}

void Assembler::fcvtslu(FRegister rd, Register rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_F));
	EmitRType(FCVTSint, FRegister(LU), rs1, rounding, rd, OPFP);
}
#endif // XLEN >= 64

void Assembler::fld(FRegister rd, Address addr)
{
	ASSERT(Supports(RV_D));
	if (Supports(RV_C)) {
		if ((addr.base() == SP) && IsCSPLoad8Imm(addr.offset())) {
			c_fldsp(rd, addr);
			return;
		}
		if (IsCFRdp(rd) && IsCRs1p(addr.base()) && IsCMem8Imm(addr.offset())) {
			c_fld(rd, addr);
			return;
		}
	}
	EmitIType(addr.offset(), addr.base(), D, rd, LOADFP);
}

void Assembler::fsd(FRegister rs2, Address addr)
{
	ASSERT(Supports(RV_D));
	if (Supports(RV_C)) {
		if ((addr.base() == SP) && IsCSPStore8Imm(addr.offset())) {
			c_fsdsp(rs2, addr);
			return;
		}
		if (IsCFRs2p(rs2) && IsCRs1p(addr.base()) && IsCMem8Imm(addr.offset())) {
			c_fsd(rs2, addr);
			return;
		}
	}
	EmitSType(addr.offset(), rs2, addr.base(), D, STOREFP);
}

void Assembler::fmaddd(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitR4Type(rs3, F2_D, rs2, rs1, rounding, rd, FMADD);
}

void Assembler::fmsubd(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitR4Type(rs3, F2_D, rs2, rs1, rounding, rd, FMSUB);
}

void Assembler::fnmsubd(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitR4Type(rs3, F2_D, rs2, rs1, rounding, rd, FNMSUB);
}

void Assembler::fnmaddd(FRegister rd, FRegister rs1, FRegister rs2, FRegister rs3, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitR4Type(rs3, F2_D, rs2, rs1, rounding, rd, FNMADD);
}

void Assembler::faddd(FRegister rd, FRegister rs1, FRegister rs2, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FADDD, rs2, rs1, rounding, rd, OPFP);
}

void Assembler::fsubd(FRegister rd, FRegister rs1, FRegister rs2, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FSUBD, rs2, rs1, rounding, rd, OPFP);
}

void Assembler::fmuld(FRegister rd, FRegister rs1, FRegister rs2, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FMULD, rs2, rs1, rounding, rd, OPFP);
}

void Assembler::fdivd(FRegister rd, FRegister rs1, FRegister rs2, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FDIVD, rs2, rs1, rounding, rd, OPFP);
}

void Assembler::fsqrtd(FRegister rd, FRegister rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FSQRTD, FRegister(0), rs1, rounding, rd, OPFP);
}

void Assembler::fsgnjd(FRegister rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_D));
	EmitRType(FSGNJD, rs2, rs1, J, rd, OPFP);
}

void Assembler::fsgnjnd(FRegister rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_D));
	EmitRType(FSGNJD, rs2, rs1, JN, rd, OPFP);
}

void Assembler::fsgnjxd(FRegister rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_D));
	EmitRType(FSGNJD, rs2, rs1, JX, rd, OPFP);
}

void Assembler::fmind(FRegister rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_D));
	EmitRType(FMINMAXD, rs2, rs1, FMIN, rd, OPFP);
}

void Assembler::fmaxd(FRegister rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_D));
	EmitRType(FMINMAXD, rs2, rs1, FMAX, rd, OPFP);
}

void Assembler::fcvtsd(FRegister rd, FRegister rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FCVTS, FRegister(1), rs1, rounding, rd, OPFP);
}

void Assembler::fcvtds(FRegister rd, FRegister rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FCVTD, FRegister(0), rs1, rounding, rd, OPFP);
}

void Assembler::feqd(Register rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_D));
	EmitRType(FCMPD, rs2, rs1, FEQ, rd, OPFP);
}

void Assembler::fltd(Register rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_D));
	EmitRType(FCMPD, rs2, rs1, FLT, rd, OPFP);
}

void Assembler::fled(Register rd, FRegister rs1, FRegister rs2)
{
	ASSERT(Supports(RV_D));
	EmitRType(FCMPD, rs2, rs1, FLE, rd, OPFP);
}

void Assembler::fclassd(Register rd, FRegister rs1)
{
	ASSERT(Supports(RV_D));
	EmitRType(FCLASSD, FRegister(0), rs1, F3_1, rd, OPFP);
}

void Assembler::fcvtwd(Register rd, FRegister rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FCVTintD, FRegister(W), rs1, rounding, rd, OPFP);
}

void Assembler::fcvtwud(Register rd, FRegister rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FCVTintD, FRegister(WU), rs1, rounding, rd, OPFP);
}

void Assembler::fcvtdw(FRegister rd, Register rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FCVTDint, FRegister(W), rs1, rounding, rd, OPFP);
}

void Assembler::fcvtdwu(FRegister rd, Register rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FCVTDint, FRegister(WU), rs1, rounding, rd, OPFP);
}

#if XLEN >= 64
void Assembler::fcvtld(Register rd, FRegister rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FCVTintD, FRegister(L), rs1, rounding, rd, OPFP);
}

void Assembler::fcvtlud(Register rd, FRegister rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FCVTintD, FRegister(LU), rs1, rounding, rd, OPFP);
}

void Assembler::fmvxd(Register rd, FRegister rs1)
{
	ASSERT(Supports(RV_D));
	EmitRType(FMVXD, FRegister(0), rs1, F3_0, rd, OPFP);
}

void Assembler::fcvtdl(FRegister rd, Register rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FCVTDint, FRegister(L), rs1, rounding, rd, OPFP);
}

void Assembler::fcvtdlu(FRegister rd, Register rs1, RoundingMode rounding)
{
	ASSERT(Supports(RV_D));
	EmitRType(FCVTDint, FRegister(LU), rs1, rounding, rd, OPFP);
}

void Assembler::fmvdx(FRegister rd, Register rs1)
{
	ASSERT(Supports(RV_D));
	EmitRType(FMVDX, FRegister(0), rs1, F3_0, rd, OPFP);
}
#endif // XLEN >= 64

#if XLEN >= 64
void Assembler::adduw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zba));
	EmitRType(ADDUW, rs2, rs1, F3_0, rd, OP32);
}
#endif

void Assembler::sh1add(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zba));
	EmitRType(SHADD, rs2, rs1, SH1ADD, rd, OP);
}

#if XLEN >= 64
void Assembler::sh1adduw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zba));
	EmitRType(SHADD, rs2, rs1, SH1ADD, rd, OP32);
}
#endif

void Assembler::sh2add(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zba));
	EmitRType(SHADD, rs2, rs1, SH2ADD, rd, OP);
}

#if XLEN >= 64
void Assembler::sh2adduw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zba));
	EmitRType(SHADD, rs2, rs1, SH2ADD, rd, OP32);
}
#endif

void Assembler::sh3add(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zba));
	EmitRType(SHADD, rs2, rs1, SH3ADD, rd, OP);
}

#if XLEN >= 64
void Assembler::sh3adduw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zba));
	EmitRType(SHADD, rs2, rs1, SH3ADD, rd, OP32);
}

void Assembler::slliuw(Register rd, Register rs1, intx_t shamt)
{
	ASSERT((shamt > 0) && (shamt < 32));
	ASSERT(Supports(RV_Zba));
	EmitRType(SLLIUW, shamt, rs1, SLLI, rd, OPIMM32);
}
#endif

void Assembler::andn(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(SUB, rs2, rs1, AND, rd, OP);
}

void Assembler::orn(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(SUB, rs2, rs1, OR, rd, OP);
}

void Assembler::xnor(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(SUB, rs2, rs1, XOR, rd, OP);
}

void Assembler::clz(Register rd, Register rs1)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(COUNT, 0b00000, rs1, F3_COUNT, rd, OPIMM);
}

void Assembler::clzw(Register rd, Register rs1)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(COUNT, 0b00000, rs1, F3_COUNT, rd, OPIMM32);
}

void Assembler::ctz(Register rd, Register rs1)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(COUNT, 0b00001, rs1, F3_COUNT, rd, OPIMM);
}

void Assembler::ctzw(Register rd, Register rs1)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(COUNT, 0b00001, rs1, F3_COUNT, rd, OPIMM32);
}

void Assembler::cpop(Register rd, Register rs1)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(COUNT, 0b00010, rs1, F3_COUNT, rd, OPIMM);
}

void Assembler::cpopw(Register rd, Register rs1)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(COUNT, 0b00010, rs1, F3_COUNT, rd, OPIMM32);
}

void Assembler::max(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(MINMAXCLMUL, rs2, rs1, MAX, rd, OP);
}

void Assembler::maxu(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(MINMAXCLMUL, rs2, rs1, MAXU, rd, OP);
}

void Assembler::min(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(MINMAXCLMUL, rs2, rs1, MIN, rd, OP);
}

void Assembler::minu(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(MINMAXCLMUL, rs2, rs1, MINU, rd, OP);
}

void Assembler::sextb(Register rd, Register rs1)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType((Funct7)0b0110000, 0b00100, rs1, SEXT, rd, OPIMM);
}

void Assembler::sexth(Register rd, Register rs1)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType((Funct7)0b0110000, 0b00101, rs1, SEXT, rd, OPIMM);
}

void Assembler::zexth(Register rd, Register rs1)
{
	ASSERT(Supports(RV_Zbb));
#if XLEN == 32
	EmitRType((Funct7)0b0000100, 0b00000, rs1, ZEXT, rd, OP);
#elif XLEN == 64
	EmitRType((Funct7)0b0000100, 0b00000, rs1, ZEXT, rd, OP32);
#else
	UNIMPLEMENTED();
#endif
}

void Assembler::rol(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(ROTATE, rs2, rs1, ROL, rd, OP);
}

void Assembler::rolw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(ROTATE, rs2, rs1, ROL, rd, OP32);
}

void Assembler::ror(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(ROTATE, rs2, rs1, ROR, rd, OP);
}

void Assembler::rori(Register rd, Register rs1, intx_t shamt)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(ROTATE, shamt, rs1, ROR, rd, OPIMM);
}

void Assembler::roriw(Register rd, Register rs1, intx_t shamt)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(ROTATE, shamt, rs1, ROR, rd, OPIMM32);
}

void Assembler::rorw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType(ROTATE, rs2, rs1, ROR, rd, OP32);
}

void Assembler::orcb(Register rd, Register rs1)
{
	ASSERT(Supports(RV_Zbb));
	EmitRType((Funct7)0b0010100, 0b00111, rs1, (Funct3)0b101, rd, OPIMM);
}

void Assembler::rev8(Register rd, Register rs1)
{
	ASSERT(Supports(RV_Zbb));
#if XLEN == 32
	EmitRType((Funct7)0b0110100, 0b11000, rs1, (Funct3)0b101, rd, OPIMM);
#elif XLEN == 64
	EmitRType((Funct7)0b0110101, 0b11000, rs1, (Funct3)0b101, rd, OPIMM);
#else
	UNIMPLEMENTED();
#endif
}

void Assembler::clmul(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbc));
	EmitRType(MINMAXCLMUL, rs2, rs1, CLMUL, rd, OP);
}

void Assembler::clmulh(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbc));
	EmitRType(MINMAXCLMUL, rs2, rs1, CLMULH, rd, OP);
}

void Assembler::clmulr(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbc));
	EmitRType(MINMAXCLMUL, rs2, rs1, CLMULR, rd, OP);
}

void Assembler::bclr(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbs));
	EmitRType(BCLRBEXT, rs2, rs1, BCLR, rd, OP);
}

void Assembler::bclri(Register rd, Register rs1, intx_t shamt)
{
	ASSERT(Supports(RV_Zbs));
	EmitRType(BCLRBEXT, shamt, rs1, BCLR, rd, OPIMM);
}

void Assembler::bext(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbs));
	EmitRType(BCLRBEXT, rs2, rs1, BEXT, rd, OP);
}

void Assembler::bexti(Register rd, Register rs1, intx_t shamt)
{
	ASSERT(Supports(RV_Zbs));
	EmitRType(BCLRBEXT, shamt, rs1, BEXT, rd, OPIMM);
}

void Assembler::binv(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbs));
	EmitRType(BINV, rs2, rs1, F3_BINV, rd, OP);
}

void Assembler::binvi(Register rd, Register rs1, intx_t shamt)
{
	ASSERT(Supports(RV_Zbs));
	EmitRType(BINV, shamt, rs1, F3_BINV, rd, OPIMM);
}

void Assembler::bset(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_Zbs));
	EmitRType(BSET, rs2, rs1, F3_BSET, rd, OP);
}

void Assembler::bseti(Register rd, Register rs1, intx_t shamt)
{
	ASSERT(Supports(RV_Zbs));
	EmitRType(BSET, shamt, rs1, F3_BSET, rd, OPIMM);
}

void Assembler::c_lwsp(Register rd, Address addr)
{
	ASSERT(rd != ZERO);
	ASSERT(addr.base() == SP);
	ASSERT(Supports(RV_C));
	Emit16(C_LWSP | EncodeCRd(rd) | EncodeCSPLoad4Imm(addr.offset()));
}

#if XLEN == 32
void Assembler::c_flwsp(FRegister rd, Address addr)
{
	ASSERT(addr.base() == SP);
	ASSERT(Supports(RV_C));
	ASSERT(Supports(RV_F));
	Emit16(C_FLWSP | EncodeCFRd(rd) | EncodeCSPLoad4Imm(addr.offset()));
}
#else
void Assembler::c_ldsp(Register rd, Address addr)
{
	ASSERT(rd != ZERO);
	ASSERT(addr.base() == SP);
	ASSERT(Supports(RV_C));
	Emit16(C_LDSP | EncodeCRd(rd) | EncodeCSPLoad8Imm(addr.offset()));
}
#endif

void Assembler::c_fldsp(FRegister rd, Address addr)
{
	ASSERT(addr.base() == SP);
	ASSERT(Supports(RV_C));
	ASSERT(Supports(RV_D));
	Emit16(C_FLDSP | EncodeCFRd(rd) | EncodeCSPLoad8Imm(addr.offset()));
}

void Assembler::c_swsp(Register rs2, Address addr)
{
	ASSERT(addr.base() == SP);
	ASSERT(Supports(RV_C));
	Emit16(C_SWSP | EncodeCRs2(rs2) | EncodeCSPStore4Imm(addr.offset()));
}

#if XLEN == 32
void Assembler::c_fswsp(FRegister rs2, Address addr)
{
	ASSERT(addr.base() == SP);
	ASSERT(Supports(RV_C));
	ASSERT(Supports(RV_F));
	Emit16(C_FSWSP | EncodeCFRs2(rs2) | EncodeCSPStore4Imm(addr.offset()));
}
#else
void Assembler::c_sdsp(Register rs2, Address addr)
{
	ASSERT(addr.base() == SP);
	ASSERT(Supports(RV_C));
	Emit16(C_SDSP | EncodeCRs2(rs2) | EncodeCSPStore8Imm(addr.offset()));
}
#endif
void Assembler::c_fsdsp(FRegister rs2, Address addr)
{
	ASSERT(addr.base() == SP);
	ASSERT(Supports(RV_C));
	ASSERT(Supports(RV_D));
	Emit16(C_FSDSP | EncodeCFRs2(rs2) | EncodeCSPStore8Imm(addr.offset()));
}

void Assembler::c_lw(Register rd, Address addr)
{
	ASSERT(Supports(RV_C));
	Emit16(C_LW | EncodeCRdp(rd) | EncodeCRs1p(addr.base()) | EncodeCMem4Imm(addr.offset()));
}

void Assembler::c_ld(Register rd, Address addr)
{
	ASSERT(Supports(RV_C));
	Emit16(C_LD | EncodeCRdp(rd) | EncodeCRs1p(addr.base()) | EncodeCMem8Imm(addr.offset()));
}

void Assembler::c_flw(FRegister rd, Address addr)
{
	ASSERT(Supports(RV_C));
	ASSERT(Supports(RV_F));
	Emit16(C_FLW | EncodeCFRdp(rd) | EncodeCRs1p(addr.base()) | EncodeCMem4Imm(addr.offset()));
}

void Assembler::c_fld(FRegister rd, Address addr)
{
	ASSERT(Supports(RV_C));
	ASSERT(Supports(RV_D));
	Emit16(C_FLD | EncodeCFRdp(rd) | EncodeCRs1p(addr.base()) | EncodeCMem8Imm(addr.offset()));
}

void Assembler::c_sw(Register rs2, Address addr)
{
	ASSERT(Supports(RV_C));
	Emit16(C_SW | EncodeCRs1p(addr.base()) | EncodeCRs2p(rs2) | EncodeCMem4Imm(addr.offset()));
}

void Assembler::c_sd(Register rs2, Address addr)
{
	ASSERT(Supports(RV_C));
	Emit16(C_SD | EncodeCRs1p(addr.base()) | EncodeCRs2p(rs2) | EncodeCMem8Imm(addr.offset()));
}

void Assembler::c_fsw(FRegister rs2, Address addr)
{
	ASSERT(Supports(RV_C));
	ASSERT(Supports(RV_F));
	Emit16(C_FSW | EncodeCRs1p(addr.base()) | EncodeCFRs2p(rs2) | EncodeCMem4Imm(addr.offset()));
}

void Assembler::c_fsd(FRegister rs2, Address addr)
{
	ASSERT(Supports(RV_C));
	ASSERT(Supports(RV_D));
	Emit16(C_FSD | EncodeCRs1p(addr.base()) | EncodeCFRs2p(rs2) | EncodeCMem8Imm(addr.offset()));
}

void Assembler::c_j(Label *label)
{
	ASSERT(Supports(RV_C));
	EmitCJump(label, C_J);
}

void Assembler::c_j(intptr_t offset)
{
	ASSERT(Supports(RV_C));
	EmitCJump(offset, C_J);
}

#if XLEN == 32
void Assembler::c_jal(Label *label)
{
	ASSERT(Supports(RV_C));
	EmitCJump(label, C_JAL);
}
#endif // XLEN == 32

void Assembler::c_jr(Register rs1)
{
	ASSERT(Supports(RV_C));
	ASSERT(rs1 != ZERO);
	Emit16(C_JR | EncodeCRs1(rs1) | EncodeCRs2(ZERO));
}

void Assembler::c_jalr(Register rs1)
{
	ASSERT(Supports(RV_C));
	Emit16(C_JALR | EncodeCRs1(rs1) | EncodeCRs2(ZERO));
}

void Assembler::c_beqz(Register rs1p, Label *label)
{
	ASSERT(Supports(RV_C));
	EmitCBranch(rs1p, label, C_BEQZ);
}

void Assembler::c_beqz(Register rs1p, intptr_t offset)
{
	ASSERT(Supports(RV_C));
	EmitCBranch(rs1p, offset, C_BEQZ);
}

void Assembler::c_bnez(Register rs1p, Label *label)
{
	ASSERT(Supports(RV_C));
	EmitCBranch(rs1p, label, C_BNEZ);
}

void Assembler::c_bnez(Register rs1p, intptr_t offset)
{
	ASSERT(Supports(RV_C));
	EmitCBranch(rs1p, offset, C_BNEZ);
}

void Assembler::c_li(Register rd, intptr_t imm)
{
	ASSERT(Supports(RV_C));
	ASSERT(rd != ZERO);
	Emit16(C_LI | EncodeCRd(rd) | EncodeCIImm(imm));
}

void Assembler::c_lui(Register rd, uintptr_t imm)
{
	ASSERT(Supports(RV_C));
	ASSERT(rd != ZERO);
	ASSERT(rd != SP);
	Emit16(C_LUI | EncodeCRd(rd) | EncodeCUImm(imm));
}

void Assembler::c_addi(Register rd, Register rs1, intptr_t imm)
{
	ASSERT(Supports(RV_C));
	ASSERT(imm != 0);
	ASSERT(rd == rs1);
	Emit16(C_ADDI | EncodeCRd(rd) | EncodeCIImm(imm));
}

#if XLEN >= 64
void Assembler::c_addiw(Register rd, Register rs1, intptr_t imm)
{
	ASSERT(Supports(RV_C));
	ASSERT(rd == rs1);
	Emit16(C_ADDIW | EncodeCRd(rd) | EncodeCIImm(imm));
}
#endif
void Assembler::c_addi16sp(Register rd, Register rs1, intptr_t imm)
{
	ASSERT(Supports(RV_C));
	ASSERT(rd == rs1);
	Emit16(C_ADDI16SP | EncodeCRd(rd) | EncodeCI16Imm(imm));
}

void Assembler::c_addi4spn(Register rdp, Register rs1, intptr_t imm)
{
	ASSERT(Supports(RV_C));
	ASSERT(rs1 == SP);
	ASSERT(imm != 0);
	Emit16(C_ADDI4SPN | EncodeCRdp(rdp) | EncodeCI4SPNImm(imm));
}

void Assembler::c_slli(Register rd, Register rs1, intptr_t imm)
{
	ASSERT(Supports(RV_C));
	ASSERT(rd == rs1);
	ASSERT(imm != 0);
	//TODO Temp fix
	imm=SignExtend(6, imm);
	Emit16(C_SLLI | EncodeCRd(rd) | EncodeCIImm(imm));
}

void Assembler::c_srli(Register rd, Register rs1, intptr_t imm)
{
	ASSERT(Supports(RV_C));
	ASSERT(rd == rs1);
	ASSERT(imm != 0);
	imm = SignExtend(6, imm);
	Emit16(C_SRLI | EncodeCRs1p(rd) | EncodeCIImm(imm));
}

void Assembler::c_srai(Register rd, Register rs1, intptr_t imm)
{
	ASSERT(Supports(RV_C));
	ASSERT(rd == rs1);
	ASSERT(imm != 0);
	imm = SignExtend(6, imm);
	Emit16(C_SRAI | EncodeCRs1p(rd) | EncodeCIImm(imm));
}

void Assembler::c_andi(Register rd, Register rs1, intptr_t imm)
{
	ASSERT(Supports(RV_C));
	ASSERT(rd == rs1);
	Emit16(C_ANDI | EncodeCRs1p(rd) | EncodeCIImm(imm));
}

void Assembler::c_mv(Register rd, Register rs2)
{
	ASSERT(Supports(RV_C));
	ASSERT(rd != ZERO);
	ASSERT(rs2 != ZERO);
	Emit16(C_MV | EncodeCRd(rd) | EncodeCRs2(rs2));
}

void Assembler::c_add(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_C));
	ASSERT(rd != ZERO);
	ASSERT(rd == rs1);
	ASSERT(rs2 != ZERO);
	Emit16(C_ADD | EncodeCRd(rd) | EncodeCRs2(rs2));
}

void Assembler::c_and(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_C));
	ASSERT(rd == rs1);
	Emit16(C_AND | EncodeCRs1p(rs1) | EncodeCRs2p(rs2));
}

void Assembler::c_or(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_C));
	Emit16(C_OR | EncodeCRs1p(rs1) | EncodeCRs2p(rs2));
}

void Assembler::c_xor(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_C));
	Emit16(C_XOR | EncodeCRs1p(rs1) | EncodeCRs2p(rs2));
}

void Assembler::c_sub(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_C));
	Emit16(C_SUB | EncodeCRs1p(rs1) | EncodeCRs2p(rs2));
}

#if XLEN >= 64
void Assembler::c_addw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_C));
	Emit16(C_ADDW | EncodeCRs1p(rs1) | EncodeCRs2p(rs2));
}

void Assembler::c_subw(Register rd, Register rs1, Register rs2)
{
	ASSERT(Supports(RV_C));
	Emit16(C_SUBW | EncodeCRs1p(rs1) | EncodeCRs2p(rs2));
}
#endif // XLEN >= 64

void Assembler::c_nop()
{
	ASSERT(Supports(RV_C));
	Emit16(C_NOP);
}

void Assembler::c_ebreak()
{
	ASSERT(Supports(RV_C));
	Emit16(C_EBREAK);
}

void Assembler::EmitBranch(Register rs1, Register rs2, Label *label, Funct3 func)
{
	intptr_t offset;
	if (label->IsBound()) {
		offset = label->Position() - Position();
	} else {
		offset = label->position_;
		label->LinkTo(Position());
	}
	EmitBType(offset, rs2, rs1, func, BRANCH);
}

void Assembler::EmitBranch(Register rs1, Register rs2, intptr_t offset, Funct3 func) { EmitBType(offset, rs2, rs1, func, BRANCH); }

void Assembler::EmitJump(Register rd, Label *label, Opcode op)
{
	intptr_t offset;
	if (label->IsBound()) {
		offset = label->Position() - Position();
	} else {
		offset = label->position_;
		label->LinkTo(Position());
	}
	EmitJType(offset, rd, JAL);
}

void Assembler::EmitJump(Register rd, intptr_t offset, Opcode op) { EmitJType(offset, rd, JAL); }

void Assembler::EmitCBranch(Register rs1p, Label *label, COpcode op)
{
	intptr_t offset;
	if (label->IsBound()) {
		offset = label->Position() - Position();
	} else {
		offset = label->position_;
		label->LinkTo(Position());
	}
	Emit16(op | EncodeCRs1p(rs1p) | EncodeCBImm(offset));
}

void Assembler::EmitCBranch(Register rs1p, intptr_t offset, COpcode op) { Emit16(op | EncodeCRs1p(rs1p) | EncodeCBImm(offset)); }

void Assembler::EmitCJump(Label *label, COpcode op)
{
	intptr_t offset;
	if (label->IsBound()) {
		offset = label->Position() - Position();
	} else {
		offset = label->position_;
		label->LinkTo(Position());
	}
	Emit16(op | EncodeCJImm(offset));
}

void Assembler::EmitCJump(intptr_t offset, COpcode op) { Emit16(op | EncodeCJImm(offset)); }

void Assembler::EmitRType(Funct5 funct5, std::memory_order order, Register rs2, Register rs1, Funct3 funct3, Register rd, Opcode opcode)
{
	intptr_t funct7 = funct5 << 2;
	switch (order) {
	case std::memory_order_acq_rel:
		funct7 |= 0b11;
		break;
	case std::memory_order_acquire:
		funct7 |= 0b10;
		break;
	case std::memory_order_release:
		funct7 |= 0b01;
		break;
	case std::memory_order_relaxed:
		funct7 |= 0b00;
		break;
	default:
		FATAL("Invalid memory order");
	}
	EmitRType((Funct7)funct7, rs2, rs1, funct3, rd, opcode);
}

void Assembler::EmitRType(Funct7 funct7, Register rs2, Register rs1, Funct3 funct3, Register rd, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeFunct7(funct7);
	e |= EncodeRs2(rs2);
	e |= EncodeRs1(rs1);
	e |= EncodeFunct3(funct3);
	e |= EncodeRd(rd);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitRType(Funct7 funct7, FRegister rs2, FRegister rs1, Funct3 funct3, FRegister rd, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeFunct7(funct7);
	e |= EncodeFRs2(rs2);
	e |= EncodeFRs1(rs1);
	e |= EncodeFunct3(funct3);
	e |= EncodeFRd(rd);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitRType(Funct7 funct7, FRegister rs2, FRegister rs1, RoundingMode round, FRegister rd, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeFunct7(funct7);
	e |= EncodeFRs2(rs2);
	e |= EncodeFRs1(rs1);
	e |= EncodeRoundingMode(round);
	e |= EncodeFRd(rd);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitRType(Funct7 funct7, FRegister rs2, Register rs1, RoundingMode round, FRegister rd, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeFunct7(funct7);
	e |= EncodeFRs2(rs2);
	e |= EncodeRs1(rs1);
	e |= EncodeRoundingMode(round);
	e |= EncodeFRd(rd);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitRType(Funct7 funct7, FRegister rs2, Register rs1, Funct3 funct3, FRegister rd, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeFunct7(funct7);
	e |= EncodeFRs2(rs2);
	e |= EncodeRs1(rs1);
	e |= EncodeFunct3(funct3);
	e |= EncodeFRd(rd);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitRType(Funct7 funct7, FRegister rs2, FRegister rs1, Funct3 funct3, Register rd, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeFunct7(funct7);
	e |= EncodeFRs2(rs2);
	e |= EncodeFRs1(rs1);
	e |= EncodeFunct3(funct3);
	e |= EncodeRd(rd);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitRType(Funct7 funct7, FRegister rs2, FRegister rs1, RoundingMode round, Register rd, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeFunct7(funct7);
	e |= EncodeFRs2(rs2);
	e |= EncodeFRs1(rs1);
	e |= EncodeRoundingMode(round);
	e |= EncodeRd(rd);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitRType(Funct7 funct7, intptr_t shamt, Register rs1, Funct3 funct3, Register rd, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeFunct7(funct7);
	e |= EncodeShamt(shamt);
	e |= EncodeRs1(rs1);
	e |= EncodeFunct3(funct3);
	e |= EncodeRd(rd);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitR4Type(FRegister rs3, Funct2 funct2, FRegister rs2, FRegister rs1, RoundingMode round, FRegister rd, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeFRs3(rs3);
	e |= EncodeFunct2(funct2);
	e |= EncodeFRs2(rs2);
	e |= EncodeFRs1(rs1);
	e |= EncodeRoundingMode(round);
	e |= EncodeFRd(rd);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitIType(intptr_t imm, Register rs1, Funct3 funct3, Register rd, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeITypeImm(imm);
	e |= EncodeRs1(rs1);
	e |= EncodeFunct3(funct3);
	e |= EncodeRd(rd);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitIType(intptr_t imm, Register rs1, Funct3 funct3, FRegister rd, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeITypeImm(imm);
	e |= EncodeRs1(rs1);
	e |= EncodeFunct3(funct3);
	e |= EncodeFRd(rd);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitSType(intptr_t imm, Register rs2, Register rs1, Funct3 funct3, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeSTypeImm(imm);
	e |= EncodeRs2(rs2);
	e |= EncodeRs1(rs1);
	e |= EncodeFunct3(funct3);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitSType(intptr_t imm, FRegister rs2, Register rs1, Funct3 funct3, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeSTypeImm(imm);
	e |= EncodeFRs2(rs2);
	e |= EncodeRs1(rs1);
	e |= EncodeFunct3(funct3);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitBType(intptr_t imm, Register rs2, Register rs1, Funct3 funct3, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeBTypeImm(imm);
	e |= EncodeRs2(rs2);
	e |= EncodeRs1(rs1);
	e |= EncodeFunct3(funct3);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitUType(intptr_t imm, Register rd, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeUTypeImm(imm);
	e |= EncodeRd(rd);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

void Assembler::EmitJType(intptr_t imm, Register rd, Opcode opcode)
{
	uint32_t e = 0;
	e |= EncodeJTypeImm(imm);
	e |= EncodeRd(rd);
	e |= EncodeOpcode(opcode);
	Emit32(e);
}

} // namespace arancini::output::dynamic::riscv64
