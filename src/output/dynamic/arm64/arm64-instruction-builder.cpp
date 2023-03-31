#include <arancini/output/dynamic/arm64/arm64-instruction-builder.h>
#include <bitset>
#include <unordered_map>
#include <array>

using namespace arancini::output::dynamic::arm64;

// #define DEBUG_REGALLOC
#define DEBUG_STREAM std::cerr

void arm64_instruction_builder::spill() {
}

void arm64_instruction_builder::emit(machine_code_writer &writer) {
    size_t size;
    uint8_t* encode;
    std::stringstream assembly;

    for (std::size_t i = 0; i < instructions_.size(); ++i) {
        const auto &insn = instructions_[i];

        if (insn.is_dead())
            continue;

        const auto &operands = insn.get_operands();

        switch (insn.opform) {
        case arm64_opform::OF_NONE:
            break;

        case arm64_opform::OF_R32:
        case arm64_opform::OF_R64:
            if (!operands[0].is_preg()) {
                throw std::runtime_error("expected preg operand 0");
            }

            break;

        // TODO: handle 32-bit regs
        case arm64_opform::OF_R32_R32:
        case arm64_opform::OF_R64_R64:
        // case arm64_opform::OF_R64_R32:
            if (!operands[0].is_preg()) {
                throw std::runtime_error("expected preg operand 0");
            }

            if (!operands[1].is_preg()) {
                throw std::runtime_error("expected preg operand 1");
            }

            break;

        case arm64_opform::OF_R32_M32:
        case arm64_opform::OF_R64_M64: {
            if (!operands[0].is_preg()) {
                throw std::runtime_error("expected preg operand 0");
            }

            if (!operands[1].is_mem()) {
                throw std::runtime_error("expected mem operand 1");
            }

            if (operands[1].memop.virt_base) {
                throw std::runtime_error("expected mem preg base operand 1");
            }

            break;
        }

        // case arm64_opform::OF_M8_R8:
        // case arm64_opform::OF_M16_R16:
        case arm64_opform::OF_M32_R32:
        case arm64_opform::OF_M64_R64: {
            if (!operands[0].is_mem()) {
                throw std::runtime_error("expected mem operand 0");
            }

            if (operands[0].memop.virt_base) {
                throw std::runtime_error("expected mem preg base operand 0");
            }

            if (!operands[1].is_preg()) {
                throw std::runtime_error("expected preg operand 1");
            }

            break;
        }

        // case arm64_opform::OF_M8_I8:
        // case arm64_opform::OF_M16_I16:
        // case arm64_opform::OF_M32_I32:
        // case arm64_opform::OF_M64_I64: {
        // 	if (!operands[0].is_mem()) {
        // 		throw std::runtime_error("expected mem operand 0");
        // 	}

        // 	if (operands[0].memop.virt_base) {
        // 		throw std::runtime_error("expected mem preg base operand 0");
        // 	}

        // 	unsigned long overridden_opcode = raw_opcode;

        // 	// switch (operands[0].memop.seg) {
        // 	// case arm64_register_names::FS:
        // 	// 	overridden_opcode |= FE_SEG(FE_FS);
        // 	// 	break;
        // 	// case arm64_register_names::GS:
        // 	// 	overridden_opcode |= FE_SEG(FE_GS);
        // 	// 	break;
        // 	// default:
        // 	// 	break;
        // 	// }

        // 	if (!operands[1].is_imm()) {
        // 		throw std::runtime_error("expected imm operand 1");
        // 	}
        // 	break;
        // }

        // case arm64_opform::OF_R8_I8:
        // case arm64_opform::OF_R16_I16:
        case arm64_opform::OF_R32_I32:
        case arm64_opform::OF_R64_I64:
            if (!operands[0].is_preg()) {
                throw std::runtime_error("expected preg operand 0");
            }

            if (!operands[1].is_imm()) {
                throw std::runtime_error("expected imm operand 1");
            }
            break;

        case arm64_opform::OF_R64_R64_I64:
            if (!operands[0].is_preg()) {
                throw std::runtime_error("expected preg operand 0");
            }

            if (!operands[1].is_preg()) {
                throw std::runtime_error("expected preg operand 1");
            }

            if (!operands[2].is_imm()) {
                throw std::runtime_error("expected imm operand 2");
            }
            break;

        default:
            (void)0;
            // throw std::runtime_error("unsupported operand form");
        }
    }

    dump(assembly);
    std::cerr << assembly.str() << '\n';

    size = asm_.assemble(assembly.str().c_str(), &encode);

    // TODO: write directly
    writer.copy_in(encode, size);

    // TODO: do zero-copy keystone
    asm_.free(encode);
}

void arm64_instruction_builder::allocate() {
	// reverse linear scan allocator
#ifdef DEBUG_REGALLOC
	DEBUG_STREAM << "REGISTER ALLOCATION" << std::endl;
#endif
    // TODO: handle direct physical register usage
    // TODO: handle different sizes

	std::unordered_map<unsigned int, unsigned int> vreg_to_preg;

    // All registers can be used except:
    // SP (x31),
    // FP (x29),
    // zero (x31)
    // Return to trampoline (x30)
    // Memory base (x18)
	std::bitset<32> avail_physregs = 0x1FFBFFFFF;

	for (auto RI = instructions_.rbegin(), RE = instructions_.rend(); RI != RE; RI++) {
		auto &insn = *RI;

#ifdef DEBUG_REGALLOC
		DEBUG_STREAM << "considering instruction ";
		insn.dump(DEBUG_STREAM);
		DEBUG_STREAM << '\n';
#endif

		// kill defs first
		for (size_t i = 0; i < insn.opcount; i++) {
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

					o.allocate(pri, 64);
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
        std::array<unsigned int, arm64_instruction::nr_operands> alloced;
		for (size_t i = 0; i < insn.opcount; i++) {
			auto &o = insn.get_operand(i);

			// We only care about REG uses - but we also need to consider REGs used in MEM expressions
			if (o.is_use() && o.is_vreg()) {
#ifdef DEBUG_REGALLOC
				DEBUG_STREAM << "  USE ";
				o.dump(DEBUG_STREAM);
                DEBUG_STREAM << '\n';
#endif

				unsigned int vri = o.vregop.index;

				if (!vreg_to_preg.count(vri)) {
					auto allocation = avail_physregs._Find_first();

                    // TODO: register spilling

					avail_physregs.flip(allocation);

					vreg_to_preg[vri] = allocation;

					o.allocate(allocation, 64);

                    alloced[i] = allocation;
#ifdef DEBUG_REGALLOC
					DEBUG_STREAM << " allocating vreg to ";
					o.dump(DEBUG_STREAM);
                    DEBUG_STREAM << '\n';
#endif
				} else {
					o.allocate(vreg_to_preg.at(vri), 64);
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
					unsigned int vri = o.memop.vbase.index;

					if (!vreg_to_preg.count(vri)) {
						auto allocation = avail_physregs._Find_first();

                        // TODO: register spilling
                        if (allocation == 30) {
                            allocation = 1; // set to x1
                            for (auto alloc : alloced) {
                                if (alloc == allocation) {
                                    allocation++;
                                    break;
                                }
                            }

                            std::cerr << "Run out of registers for block\n";
                        }

						avail_physregs.flip(allocation);

						vreg_to_preg[vri] = allocation;

                        // TODO size
						o.memop.pbase = arm64_physreg_op(allocation, 64);
						o.allocate_base(allocation, 64);

#ifdef DEBUG_REGALLOC
						DEBUG_STREAM << " allocating vreg to ";
						o.dump(DEBUG_STREAM);
                        DEBUG_STREAM << '\n';
#endif
					} else {
						o.allocate_base(vreg_to_preg.at(vri), 64);
					}
				}
			}
		}

		// get uses, allocate and make live
		// get defs, make free

		// Kill MOVs
        if (insn.opcode.find("mov") != std::string::npos) {
            if (insn.operands[0].pregop.get() == insn.operands[1].pregop.get()) {
                insn.kill();
            }
        }
	}
}

void arm64_instruction_builder::dump(std::ostream &os) const {
	for (const auto &insn : instructions_) {
		if (!insn.is_dead()) {
            insn.dump(os);
            os << '\n';
		}
	}
}

