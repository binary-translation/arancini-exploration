#include <arancini/output/dynamic/x86/x86-instruction.h>

using namespace arancini::output::dynamic::x86;

x86_register x86_register::al(0, 8, false, false);
x86_register x86_register::ah(4, 8, false, false);
x86_register x86_register::ax(0, 16, false, false);
x86_register x86_register::eax(0, 32, false, false);
x86_register x86_register::rax(0, 64, false, false);

x86_register x86_register::cl(1, 8, false, false);
x86_register x86_register::ch(5, 8, false, false);
x86_register x86_register::cx(1, 16, false, false);
x86_register x86_register::ecx(1, 32, false, false);
x86_register x86_register::rcx(1, 64, false, false);

x86_register x86_register::dl(2, 8, false, false);
x86_register x86_register::dh(6, 8, false, false);
x86_register x86_register::dx(2, 16, false, false);
x86_register x86_register::edx(2, 32, false, false);
x86_register x86_register::rdx(2, 64, false, false);

x86_register x86_register::bl(3, 8, false, false);
x86_register x86_register::bh(7, 8, false, false);
x86_register x86_register::bx(3, 16, false, false);
x86_register x86_register::ebx(3, 32, false, false);
x86_register x86_register::rbx(3, 64, false, false);

x86_register x86_register::spl(4, 8, false, true);
x86_register x86_register::sp(4, 16, false, false);
x86_register x86_register::esp(4, 32, false, false);
x86_register x86_register::rsp(4, 64, false, false);

x86_register x86_register::bpl(5, 8, false, true);
x86_register x86_register::bp(5, 16, false, false);
x86_register x86_register::ebp(5, 32, false, false);
x86_register x86_register::rbp(5, 64, false, false);

x86_register x86_register::sil(6, 8, false, true);
x86_register x86_register::si(6, 16, false, false);
x86_register x86_register::esi(6, 32, false, false);
x86_register x86_register::rsi(6, 64, false, false);

x86_register x86_register::dil(7, 8, false, true);
x86_register x86_register::di(7, 16, false, false);
x86_register x86_register::edi(7, 32, false, false);
x86_register x86_register::rdi(7, 64, false, false);

// --- High Regs --- //

x86_register x86_register::r8b(0, 8, true, false);
x86_register x86_register::r8w(0, 16, true, false);
x86_register x86_register::r8l(0, 32, true, false);
x86_register x86_register::r8(0, 64, true, false);

x86_register x86_register::r9b(1, 8, true, false);
x86_register x86_register::r9w(1, 16, true, false);
x86_register x86_register::r9l(1, 32, true, false);
x86_register x86_register::r9(1, 64, true, false);

x86_register x86_register::r10b(2, 8, true, false);
x86_register x86_register::r10w(2, 16, true, false);
x86_register x86_register::r10l(2, 32, true, false);
x86_register x86_register::r10(2, 64, true, false);

x86_register x86_register::r11b(3, 8, true, false);
x86_register x86_register::r11w(3, 16, true, false);
x86_register x86_register::r11l(3, 32, true, false);
x86_register x86_register::r11(3, 64, true, false);

x86_register x86_register::r12b(4, 8, true, false);
x86_register x86_register::r12w(4, 16, true, false);
x86_register x86_register::r12l(4, 32, true, false);
x86_register x86_register::r12(4, 64, true, false);

x86_register x86_register::r13b(5, 8, true, false);
x86_register x86_register::r13w(5, 16, true, false);
x86_register x86_register::r13l(5, 32, true, false);
x86_register x86_register::r13(5, 64, true, false);

x86_register x86_register::r14b(6, 8, true, false);
x86_register x86_register::r14w(6, 16, true, false);
x86_register x86_register::r14l(6, 32, true, false);
x86_register x86_register::r14(6, 64, true, false);

x86_register x86_register::r15b(7, 8, true, false);
x86_register x86_register::r15w(7, 16, true, false);
x86_register x86_register::r15l(7, 32, true, false);
x86_register x86_register::r15(7, 64, true, false);

// --- Special Regs --- //
x86_register x86_register::riz(0, 0, false, false);
x86_register x86_register::rip(0, 0, false, false);

void x86_instruction::emit(machine_code_writer &writer) const
{
	x86_raw_instruction ri;

	switch (opcode_) {
	case opcodes::mov:
		switch (operands[0].type) {
		case operand_type::regop:
			switch (operands[1].type) {
			case operand_type::regop: // mov reg:r, reg:rm
				ri.insn_encoding = encoding::rm;

				ri.modrm.parts.mod = 3;
				ri.modrm.parts.reg = operands[0].reg.index();
				ri.modrm.parts.rm = operands[1].reg.index();

				if (operands[0].reg.width() == 8) {
					ri.insn_opcode = raw_opcodes::mov_r8_rm8;
				} else {
					ri.insn_opcode = raw_opcodes::mov_r16_32_64_rm16_32_64;
				}

				break;

			case operand_type::memory: // mov reg, mem
				ri.insn_encoding = encoding::rm;

				if (operands[1].mem.displacement() == 0) {
					ri.modrm.parts.mod = 0;
				} else {
					ri.modrm.parts.mod = 1;
					ri.displacement = operands[1].mem.displacement();
				}

				ri.modrm.parts.reg = operands[0].reg.index();
				ri.modrm.parts.rm = operands[1].mem.base().index();

				if (operands[0].reg.width() == 8) {
					ri.insn_opcode = raw_opcodes::mov_r8_rm8;
				} else {
					ri.insn_opcode = raw_opcodes::mov_r16_32_64_rm16_32_64;
				}
				break;

			case operand_type::immediate: // mov reg, imm
				ri.insn_encoding = encoding::oi;

				if (operands[0].reg.width() == 8) {
					ri.insn_opcode = (raw_opcodes)((int)raw_opcodes::mov_r8_imm8 + operands[0].reg.index());
				} else {
					ri.insn_opcode = (raw_opcodes)((int)raw_opcodes::mov_r16_32_64_imm16_32_64 + operands[0].reg.index());
				}

				ri.immediate = operands[1].imm.value();
				ri.imm_width = operands[1].imm.width();

				break;

			default:
				throw std::runtime_error("unsupported source operand type");
			}
			break;

		case operand_type::memory:
			switch (operands[1].type) {
			case operand_type::regop: // mov mem, reg
				ri.insn_encoding = encoding::mr;

				if (operands[0].mem.displacement() == 0) {
					ri.modrm.parts.mod = 0;
				} else {
					ri.modrm.parts.mod = 1;
					ri.displacement = operands[0].mem.displacement();
				}

				ri.modrm.parts.mod = 0;
				ri.modrm.parts.reg = operands[1].reg.index();
				ri.modrm.parts.rm = operands[0].mem.base().index();

				if (operands[0].reg.width() == 8) {
					ri.insn_opcode = raw_opcodes::mov_rm8_r8;
				} else {
					ri.insn_opcode = raw_opcodes::mov_rm16_32_64_r16_32_64;
				}

				break;

			case operand_type::immediate: // mov mem, imm
				ri.insn_encoding = encoding::mi;

				if (operands[1].imm.width() == 8) {
					ri.insn_opcode = raw_opcodes::mov_rm8_imm8;
				} else if (operands[1].imm.width() == 64) {
					throw std::runtime_error("illegal immediate width");
				} else {
					ri.insn_opcode = raw_opcodes::mov_rm16_32_64_imm16_32;
				}

				ri.immediate = operands[1].imm.value();
				ri.imm_width = operands[1].imm.width();

				break;

			default:
				throw std::runtime_error("unsupported source operand type");
			}
			break;

		default:
			throw std::runtime_error("unsupported destination operand type");
		}
		break;

	default:
		ri.insn_opcode = raw_opcodes::nop;
		ri.insn_encoding = encoding::zo;
		break;
	}

	ri.emit(writer);
}
