#include <arancini/output/dynamic/arm64/arm64-instruction-builder.h>
#include <bitset>
#include <unordered_map>

using namespace arancini::output::dynamic::arm64;

// #define DEBUG_REGALLOC
#define DEBUG_STREAM std::cerr

void arm64_instruction_builder::allocate() {
	// reverse linear scan allocator
#ifdef DEBUG_REGALLOC
	DEBUG_STREAM << "REGISTER ALLOCATION" << std::endl;
#endif

	std::unordered_map<unsigned int, unsigned int> vreg_to_preg;
	std::bitset<6> avail_physregs = 0x3f;

	for (auto RI = instructions_.rbegin(), RE = instructions_.rend(); RI != RE; RI++) {
		auto &insn = *RI;

#ifdef DEBUG_REGALLOC
		DEBUG_STREAM << "considering instruction ";
		insn.dump(DEBUG_STREAM);
#endif

		// kill defs first
		for (size_t i = 0; i < arm64_instruction::nr_operands; i++) {
			auto &o = insn.get_operand(i);

			// Only regs can be /real/ defs
			if (o.is_def() && o.is_vreg() && !o.is_use()) {
#ifdef DEBUG_REGALLOC
				DEBUG_STREAM << "  DEF ";
				o.dump(DEBUG_STREAM);
#endif

				unsigned int vri = o.vregop.index;

				auto alloc = vreg_to_preg.find(vri);

				if (alloc != vreg_to_preg.end()) {
					int pri = alloc->second;

					avail_physregs.set(pri);

					o.allocate(pri);
#ifdef DEBUG_REGALLOC
					DEBUG_STREAM << " allocated to ";
					o.dump(DEBUG_STREAM);
					DEBUG_STREAM << " -- releasing";
#endif
				} else {
#ifdef DEBUG_REGALLOC
					DEBUG_STREAM << " not allocated - killing instruction" << std::endl;
#endif
					insn.kill();
					break;
				}

#ifdef DEBUG_REGALLOC
				DEBUG_STREAM << std::endl;
#endif
			}
		}

		if (insn.is_dead()) {
			continue;
		}

		// alloc uses next
		for (size_t i = 0; i < arm64_instruction::nr_operands; i++) {
			auto &o = insn.get_operand(i);

			// We only care about REG uses - but we also need to consider REGs used in MEM expressions
			if (o.is_use() && o.is_vreg()) {
#ifdef DEBUG_REGALLOC
				DEBUG_STREAM << "  USE ";
				o.dump(DEBUG_STREAM);
#endif

				unsigned int vri = o.vregop.index;

				if (!vreg_to_preg.count(vri)) {
					auto allocation = avail_physregs._Find_first();
					avail_physregs.flip(allocation);

					vreg_to_preg[vri] = allocation;

					o.allocate(allocation);

#ifdef DEBUG_REGALLOC
					DEBUG_STREAM << " allocating vreg to ";
					o.dump(DEBUG_STREAM);
#endif
				} else {
					o.allocate(vreg_to_preg.at(vri));
				}

#ifdef DEBUG_REGALLOC
				DEBUG_STREAM << std::endl;
#endif
			} else if (o.is_mem()) {
#ifdef DEBUG_REGALLOC
				DEBUG_STREAM << "  USE ";
				o.dump(DEBUG_STREAM);
				DEBUG_STREAM << std::endl;
#endif

				if (o.memop.virt_base) {
					unsigned int vri = o.memop.vbase;

					if (!vreg_to_preg.count(vri)) {
						auto allocation = avail_physregs._Find_first();
						avail_physregs.flip(allocation);

						vreg_to_preg[vri] = allocation;

						// o.mem.base().regname = (x86_register::x86_regname)allocation;
						o.allocate_base(allocation);

#ifdef DEBUG_REGALLOC
						DEBUG_STREAM << " allocating vreg to ";
						o.dump(DEBUG_STREAM);
#endif
					} else {
						o.allocate_base(vreg_to_preg.at(vri));
					}
				}
			}
		}

		// get uses, allocate and make live
		// get defs, make free

		// Kill MOVs
	    //	switch (insn.raw_opcode) {
	    //	case FE_MOV8rr:
	    //	case FE_MOV16rr:
	    //	case FE_MOV32rr:
	    //	case FE_MOV64rr:
	    //		if (insn.operands[0].pregop.regname == insn.operands[1].pregop.regname) {
	    //			insn.kill();
	    //		}

		// default:
		// 	break;
		// }
	}
}

void arm64_instruction_builder::dump(std::ostream &os) const
{
	for (auto &insn : instructions_) {
		// if (!insn.is_dead()) {
		insn.dump(os);
		//}
	}
}

