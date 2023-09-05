#include "arancini/output/dynamic/arm64/arm64-instruction.h"
#include <arancini/output/dynamic/arm64/arm64-instruction-builder.h>
#include <array>
#include <bitset>
#include <utility>
#include <stdexcept>
#include <unordered_map>

using namespace arancini::output::dynamic::arm64;

/* #define DEBUG_REGALLOC */
#define DEBUG_STREAM std::cerr

void instruction_builder::spill() {
}

void instruction_builder::emit(machine_code_writer &writer) {
    size_t size;
    uint8_t* encode;
    std::stringstream assembly;

    for (std::size_t i = 0; i < instructions_.size(); ++i) {
        const auto &insn = instructions_[i];

        if (insn.is_dead())
            continue;

        const auto &operands = insn.operands();

        for (size_t i = 0; i < insn.operand_count(); ++i) {
            const auto &op = operands[i];
            if (op.type() == operand_type::invalid ||
                op.type() == operand_type::vreg ||
                (op.type() == operand_type::mem && op.memory().is_virtual())) {
                dump(assembly);
                std::cerr << assembly.str() << '\n';
                throw std::runtime_error("Virtual register after register allocation: "
                                         + insn.dump());
            }
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

void instruction_builder::allocate() {
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
	std::bitset<32> avail_float_physregs = 0xFFFFFFFFF;

	for (auto RI = instructions_.rbegin(), RE = instructions_.rend(); RI != RE; RI++) {
		auto &insn = *RI;

#ifdef DEBUG_REGALLOC
		DEBUG_STREAM << "considering instruction ";
		insn.dump(DEBUG_STREAM);
		DEBUG_STREAM << '\n';
#endif

        std::array<std::pair<size_t, size_t>, 5> allocs;
        bool has_unused_keep = false;

        auto allocate = [&allocs, &avail_physregs,
                         &avail_float_physregs, &vreg_to_preg]
                                  (operand &o, size_t idx) -> void
        {
                unsigned int vri;
                ir::value_type type;
                if (o.is_vreg()) {
                    vri = o.vreg().index();
                    type = o.vreg().type();
                } else if (o.is_mem()) {
                    auto vreg = o.memory().vreg_base();
                    vri = vreg.index();
                    type = vreg.type();
                } else {
                    throw std::runtime_error("Trying to allocate non-virtual register operand");
                }

                size_t allocation = 0;
                if (type.is_floating_point()) {
                    allocation = avail_float_physregs._Find_first();
                    avail_float_physregs.flip(allocation);
                } else {
                    allocation = avail_physregs._Find_first();
                    avail_physregs.flip(allocation);
                }

                // TODO: register spilling
                vreg_to_preg[vri] = allocation;

                if (o.is_mem())
                    o.allocate_base(allocation, type);
                else
                    o.allocate(allocation, type);

                allocs[idx] = std::make_pair(vri, allocation);
        };

		// kill defs first
		for (size_t i = 0; i < insn.operand_count(); i++) {
			auto &o = insn.operands()[i];

			// Only regs can be /real/ defs
			if (o.is_def() && o.is_vreg() && !o.is_use()) {
#ifdef DEBUG_REGALLOC
				DEBUG_STREAM << "  DEF ";
				o.dump(DEBUG_STREAM);
#endif

                auto type = o.vreg().type();
				unsigned int vri = o.vreg().index();

				auto alloc = vreg_to_preg.find(vri);

				if (alloc != vreg_to_preg.end()) {
					int pri = alloc->second;

                    if (type.is_floating_point()) {
                        avail_float_physregs.set(pri);
                    } else {
                        avail_physregs.set(pri);
                    }

					o.allocate(pri, type);
#ifdef DEBUG_REGALLOC
					DEBUG_STREAM << " allocated to ";
					o.dump(DEBUG_STREAM);
					DEBUG_STREAM << " -- releasing\n";
#endif
                } else if (o.is_keep()) {
                    has_unused_keep = true;

                    allocate(o, i);
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
		for (size_t i = 0; i < insn.operand_count(); i++) {
			auto &o = insn.operands()[i];

			// We only care about REG uses - but we also need to consider REGs used in MEM expressions
			if (o.is_use() && o.is_vreg()) {
#ifdef DEBUG_REGALLOC
				DEBUG_STREAM << "  USE ";
				o.dump(DEBUG_STREAM);
                DEBUG_STREAM << '\n';
#endif

                auto type = o.vreg().type();
                unsigned int vri = o.vreg().index();
				if (!vreg_to_preg.count(vri)) {
                    allocate(o, i);
#ifdef DEBUG_REGALLOC
					DEBUG_STREAM << " allocating vreg to ";
					o.dump(DEBUG_STREAM);
                    DEBUG_STREAM << '\n';
#endif
				} else {
					o.allocate(vreg_to_preg.at(vri), type);
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

				if (o.memory().is_virtual()) {
                    unsigned int vri = o.memory().vreg_base().index();

					if (!vreg_to_preg.count(vri)) {
                        allocate(o, i);
#ifdef DEBUG_REGALLOC
						DEBUG_STREAM << " allocating vreg to ";
						o.dump(DEBUG_STREAM);
                        DEBUG_STREAM << '\n';
#endif
					} else {
                        auto type = o.memory().vreg_base().type();
						o.allocate_base(vreg_to_preg.at(vri), type);
					}
				}
			}
		}

        if (has_unused_keep) {
            for (size_t i = 0; i < insn.operand_count(); i++) {
                const auto &op = insn.operands()[i];
                if (op.is_keep()) {
                    vreg_to_preg.erase(allocs[i].first);
                    avail_physregs.flip(allocs[i].second);
                }
            }
        }

		// Kill MOVs
        // TODO: refactor
        if (insn.opcode().find("mov") != std::string::npos) {
            operand op1 = insn.operands()[0];
            operand op2 = insn.operands()[1];

            if (op1.is_preg() && op2.is_preg()) {
                if (op1.preg().register_index() == op2.preg().register_index()) {
                    insn.kill();
                }
            }
        }
	}
}

void instruction_builder::dump(std::ostream &os) const {
	for (const auto &insn : instructions_) {
		if (!insn.is_dead()) {
            insn.dump(os);
            os << '\n';
		}
	}
}

