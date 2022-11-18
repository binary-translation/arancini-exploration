#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/x86/machine-code-builder.h>
#include <bitset>
#include <iostream>
#include <set>
#include <unordered_map>

using namespace arancini::output::dynamic;
using namespace arancini::output::dynamic::x86;

static const char *mnemonics[] = { "invalid", "mov", "movz", "movs", "and", "or", "xor", "add", "sub", "setz", "seto", "setc", "sets" };

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
	if (dead()) {
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

#define DEBUG_STREAM std::cerr
// #define DEBUG_REGALLOC

void machine_code_builder::allocate()
{
	// reverse linear scan allocator
#ifdef DEBUG_REGALLOC
	DEBUG_STREAM << "REGISTER ALLOCATION" << std::endl;
#endif

	std::unordered_map<int, physreg> vreg_to_preg;
	std::bitset<6> avail_physregs = 0x3f;

	for (auto RI = instructions_.rbegin(), RE = instructions_.rend(); RI != RE; RI++) {
		auto &insn = *RI;

#ifdef DEBUG_REGALLOC
		DEBUG_STREAM << "considering instruction ";
		insn.dump(DEBUG_STREAM);
#endif

		// kill defs first
		for (int i = 0; i < instruction::NR_OPERANDS; i++) {
			auto &o = insn.get_operand(i);

			// Only regs can be /real/ defs
			if (o.is_def() && o.oper().kind == operand_kind::reg) {
#ifdef DEBUG_REGALLOC
				DEBUG_STREAM << "  DEF ";
				o.oper().dump(DEBUG_STREAM);
#endif

				if (o.oper().reg_i.rr.kind == regref_kind::virt) {
					int vri = o.oper().reg_i.rr.vreg_i;

					auto alloc = vreg_to_preg.find(vri);

					if (alloc != vreg_to_preg.end()) {
						physreg i = alloc->second;

#ifdef DEBUG_REGALLOC
						DEBUG_STREAM << " allocated to " << regnames[(int)i] << " -- releasing";
#endif

						avail_physregs.set((int)i);

						o.oper().reg_i.rr.allocate(i);
					} else {
#ifdef DEBUG_REGALLOC
						DEBUG_STREAM << " not allocated - killing instruction" << std::endl;
#endif
						insn.kill();
						break;
					}
				}

#ifdef DEBUG_REGALLOC
				DEBUG_STREAM << std::endl;
#endif
			}
		}

		if (insn.dead()) {
			continue;
		}

		// alloc uses next
		for (int i = 0; i < instruction::NR_OPERANDS; i++) {
			auto &o = insn.get_operand(i);

			// We only care about REG uses - but we also need to consider REGs used in MEM expressions
			if (o.is_use() && o.oper().kind == operand_kind::reg) {
#ifdef DEBUG_REGALLOC
				DEBUG_STREAM << "  USE ";
				o.oper().dump(std::cerr);
#endif

				if (o.oper().reg_i.rr.kind == regref_kind::virt) {
					int vri = o.oper().reg_i.rr.vreg_i;

					if (!vreg_to_preg.count(vri)) {
						auto allocation = avail_physregs._Find_first();
						avail_physregs.flip(allocation);

						vreg_to_preg[vri] = (physreg)allocation;

#ifdef DEBUG_REGALLOC
						DEBUG_STREAM << " allocating vreg to " << regnames[allocation];
#endif

						o.oper().reg_i.rr.allocate((physreg)allocation);
					}
				}

#ifdef DEBUG_REGALLOC
				DEBUG_STREAM << std::endl;
#endif
			} else if (o.oper().kind == operand_kind::mem) {
#ifdef DEBUG_REGALLOC
				DEBUG_STREAM << "  USE ";
				o.oper().mem_i.base.dump(std::cerr);
				DEBUG_STREAM << std::endl;
#endif
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
