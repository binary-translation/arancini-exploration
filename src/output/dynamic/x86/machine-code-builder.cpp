#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/x86/machine-code-builder.h>
#include <bitset>
#include <iostream>
#include <set>
#include <unordered_map>

using namespace arancini::output::dynamic;
using namespace arancini::output::dynamic::x86;

static const char *mnemonics[] = { "invalid", "mov", "and", "or", "xor", "add", "sub", "setz", "seto", "setc", "sets" };

void instruction::dump(std::ostream &os) const
{
	if ((int)opcode_ > (sizeof(mnemonics) / sizeof(mnemonics[0]))) {
		os << "???";
	} else {
		os << mnemonics[(int)opcode_];
	}

	for (int i = 0; i < NR_OPERANDS; i++) {
		if (operands_[i].oper().kind == operand_kind::none) {
			continue;
		}

		if (i > 0) {
			os << ", ";

		} else {
			os << " ";
		}

		operands_[i].dump(os);
	}

	os << std::endl;
}

void instruction::emit(machine_code_writer &writer) const
{
	if (opcode_ == opcodes::invalid) {
		return;
	}

	writer.emit8(0xcc);
}

void operand::dump(std::ostream &os) const
{
	switch (kind) {
	case operand_kind::none:
		break;

	case operand_kind::reg:
		reg_i.rr.dump(os);
		break;

	case operand_kind::mem:
		if (mem_i.displacement) {
			os << std::dec << mem_i.displacement;
		}

		os << "(";
		mem_i.base.dump(os);
		os << ")";
		break;

	case operand_kind::immediate:
		os << "$0x" << std::hex << imm_i.val;
		break;
	}

	os << ":" << std::dec << width;
}

void instruction_operand::dump(std::ostream &os) const { operand_.dump(os); }

static const char *regnames[] = { "rax", "rcx", "rdx", "rbx", "rsi", "rdi", "rsp", "rbp" };

void regref::dump(std::ostream &os) const
{
	os << "%";

	switch (kind) {
	case regref_kind::phys:
		os << regnames[(int)preg_i];
		break;

	case regref_kind::virt:
		os << "V" << std::dec << vreg_i;
		break;
	}
}

void machine_code_builder::allocate()
{
	// reverse linear scan allocator

	std::cerr << "REGISTER ALLOCATION" << std::endl;

	std::unordered_map<int, physreg> vreg_to_preg;
	std::unordered_map<physreg, int> preg_to_vreg;
	std::bitset<6> avail_physregs;

	avail_physregs = 0x3f;

	for (auto RI = instructions_.rbegin(), RE = instructions_.rend(); RI != RE; RI++) {
		auto &insn = *RI;

		std::cerr << "considering instruction ";
		insn.dump(std::cerr);

		// kill defs first
		for (int i = 0; i < instruction::NR_OPERANDS; i++) {
			auto &o = insn.get_operand(i);

			// Only regs can be /real/ defs
			if (o.is_def() && o.oper().kind == operand_kind::reg) {
				std::cerr << "  DEF ";
				o.oper().dump(std::cerr);

				if (o.oper().reg_i.rr.kind == regref_kind::virt) {
					int vri = o.oper().reg_i.rr.vreg_i;

					auto alloc = vreg_to_preg.find(vri);

					if (alloc != vreg_to_preg.end()) {
						physreg i = alloc->second;

						std::cerr << " allocated to " << (int)i;
						avail_physregs.set((int)i);
						preg_to_vreg.erase(i);

						o.oper().reg_i.rr.kind = regref_kind::phys;
						o.oper().reg_i.rr.preg_i = i;
					} else {
						std::cerr << " not allocated";
					}
				}

				std::cerr << std::endl;
			}
		}

		// alloc uses next
		for (int i = 0; i < instruction::NR_OPERANDS; i++) {
			auto &o = insn.get_operand(i);

			// We only care about REG uses - but we also need to consider REGs used in MEM expressions
			if (o.is_use() && o.oper().kind == operand_kind::reg) {
				std::cerr << "  USE ";
				o.oper().dump(std::cerr);

				if (o.oper().reg_i.rr.kind == regref_kind::virt) {
					int vri = o.oper().reg_i.rr.vreg_i;

					if (!vreg_to_preg.count(vri)) {

						auto allocation = avail_physregs._Find_first();
						avail_physregs.flip(allocation);

						vreg_to_preg[vri] = (physreg)allocation;
						preg_to_vreg[(physreg)allocation] = vri;

						std::cerr << " allocating vreg to " << allocation;

						o.oper().reg_i.rr.kind = regref_kind::phys;
						o.oper().reg_i.rr.preg_i = (physreg)allocation;
					}
				}

				std::cerr << std::endl;
			} else if (o.oper().kind == operand_kind::mem) {
				std::cerr << "  USE ";
				o.oper().mem_i.base.dump(std::cerr);
				std::cerr << std::endl;
			}
		}

		// get uses, allocate and make live
		// get defs, make free
	}
}

void machine_code_builder::emit(machine_code_writer &writer)
{
	for (const auto &i : instructions_) {
		i.emit(writer);
	}
}
