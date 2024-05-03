#include <arancini/output/dynamic/arm64/arm64-instruction-builder.h>

#include <arancini/output/dynamic/arm64/arm64-instruction.h>
#include <arancini/output/dynamic/arm64/arm64-translation-context.h>
#include <array>
#include <bitset>
#include <exception>
#include <utility>
#include <stdexcept>
#include <unordered_map>

using namespace arancini::output::dynamic::arm64;

void instruction_builder::spill() {
}

void instruction_builder::emit(machine_code_writer &writer) {
    size_t size;
    uint8_t* encode;
    for (std::size_t i = 0; i < instructions_.size(); ++i) {
        const auto &insn = instructions_[i];

        if (insn.is_dead())
            continue;

        const auto &operands = insn.operands();

        for (const auto &op : operands) {
            bool has_virtual = false;
            // std::visit([&has_virtual](auto &&op) {
            //     using T = std::decay_t<decltype(op)>;
            //     if constexpr (std::is_same_v<T, register_operand>) {
            //          if (op.is_virtual()) has_virtual = true; 
            //     }
            //     if constexpr (std::is_same_v<T, memory_operand>) {
            //          if (op.base_register().is_virtual()) has_virtual = true; 
            //     }
            // }, op);

            if (has_virtual) {
                throw arm64_exception("Register allocation failed\nCurrent instruction stream:\n{}\n"
                                      "Virtual register after register allocation in instruction {}\n",
                                      fmt::join(instructions(), "\n"), insn);
            }
        }
    }

    auto assembly = fmt::format("{}", fmt::join(instructions(), "\n"));

    size = asm_.assemble(assembly.c_str(), &encode);

    util::global_logger.debug("Generated translation:\n{}\n", assembly);

    // TODO: write directly
    writer.copy_in(encode, size);

    // TODO: do zero-copy keystone
    asm_.free(encode);
}

struct register_set {
public:
    register_set(std::uint32_t available_scalar_registers, std::uint32_t available_neon_registers):
        scalar_registers_(available_scalar_registers),
        neon_registers_(available_neon_registers)
    { }

    using base_type = std::bitset<register_operand::physical_register_count>;

    // TODO: should return register operand
    register_operand allocate(arancini::ir::value_type t) {
        base_type *allocation_base = nullptr;
        if (t.type_class() == arancini::ir::value_type_class::signed_integer ||
            t.type_class() == arancini::ir::value_type_class::unsigned_integer) 
        {
            allocation_base = &scalar_registers_;
        } else if (t.type_class() == arancini::ir::value_type_class::floating_point) {
            allocation_base = &float_registers_;
        }

        auto index = allocation_base->_Find_first();
        allocation_base->flip(index);

        return {index, t};
    }
private:
    std::bitset<register_operand::physical_register_count> scalar_registers_;
    std::bitset<register_operand::physical_register_count> neon_registers_;
    std::bitset<register_operand::physical_register_count>& float_registers_ = neon_registers_;
};

static bool is_virtual_register(const operand::base_type operand) {
    if (std::holds_alternative<register_operand>(operand)) {
        return std::get<register_operand>(operand).is_virtual();
    }

    return false;
}

void instruction_builder::allocate() {
	// reverse linear scan allocator
    util::global_logger.debug("Performing register allocation for generated instructions\n");

    // All registers can be used except:
    // SP (x31),
    // FP (x29),
    // zero (x31)
    // Return to trampoline (x30)
    // Memory base (x18)
    // FIXME
    register_set regset(0x1FBFFFFF, 0xFFFFFFFF);

	std::unordered_map<register_operand::register_index, register_operand::register_index> vreg_to_reg;

    // TODO: replace with static map
    std::array<std::pair<std::size_t, std::size_t>, 5> current_allocations;

	for (auto RI = instructions_.rbegin(), RE = instructions_.rend(); RI != RE; RI++) {
		auto &insn = *RI;

        util::global_logger.debug("Considering instruction {}\n", insn);

        bool has_unused_keep = false;

 //        auto allocate = [&allocs, &avail_physregs,
 //                         &avail_float_physregs, &vreg_to_preg]
 //                                  (operand &o, size_t idx) -> void
 //        {
 //                unsigned int vri;
 //                ir::value_type type;
 //                if (o.is_vreg()) {
 //                    vri = o.vreg().index();
 //                    type = o.vreg().type();
 //                } else if (o.is_mem()) {
 //                    auto vreg = o.memory().vreg_base();
 //                    vri = vreg.index();
 //                    type = vreg.type();
 //                } else {
 //                    throw std::runtime_error("Trying to allocate non-virtual register operand");
 //                }
	//
 //                size_t allocation = 0;
 //                if (type.is_floating_point()) {
 //                    allocation = avail_float_physregs._Find_first();
 //                    avail_float_physregs.flip(allocation);
 //                } else {
 //                    allocation = avail_physregs._Find_first();
 //                    avail_physregs.flip(allocation);
 //                }
	//
 //                // TODO: register spilling
 //                vreg_to_preg[vri] = allocation;
	//
 //                if (o.is_mem())
 //                    o.allocate_base(allocation, type);
 //                else
 //                    o.allocate(allocation, type);
	//
 //                allocs[idx] = std::make_pair(vri, allocation);
 //        };
	//
	// 	// kill defs first
		for (std::size_t i = 0; i < insn.operands().size(); i++) {
			auto &o = insn.operands()[i];

			// Only regs can be /real/ defs
            if (const auto *vreg = std::get_if<register_operand>(&o.get()); 
                vreg && vreg->is_virtual() && o.is_def() && !o.is_use()) 
            {
                util::global_logger.debug("Defining value {}\n", o);

				auto previous_allocation = vreg_to_reg.find(vreg->index());
                register_operand::register_index allocation_index = 0;
				if (previous_allocation != vreg_to_reg.end() || insn.is_keep()) {
                    auto reg = regset.allocate(vreg->type());
                    allocation_index = reg.index();
                    o = reg;

                    util::global_logger.debug("Allocated to {}\n", o);

                    has_unused_keep = insn.is_keep();
				} else {
                    util::global_logger.debug("Value not allocated - killing instruction {}\n", insn);
					insn.kill();
					break;
				}

                current_allocations[i] = std::make_pair(allocation_index, vreg->index());
			}
		}


		if (insn.is_dead()) {
			continue;
		}

		// alloc uses next
		for (std::size_t i = 0; i < insn.operands().size(); i++) {
			auto &o = insn.operands()[i];

			// We only care about REG uses - but we also need to consider REGs used in MEM expressions
			// if (const auto *vreg = std::get_if<register_operand>(&o.get()); o.is_use() && vreg) {
   //              util::global_logger.debug("Using value {}\n", o);
   //              auto type = vreg->type();
			// 	if (!vreg_to_reg.count(vreg->index())) {
   //                  auto allocation = regset.allocate(vreg->type());
   //                  o = register_operand(allocation, vreg->type());
   //                  util::global_logger.debug("Allocating virtual register to {}\n", o);
			// 	} else {
   //                  // TODO
			// 		o.allocate(vreg_to_preg.at(vri), type);
			// 	}
			// } else if (o.is_mem()) {
   //              util::global_logger.debug("Using value {}\n", o);
			//
			// 	if (auto *mem = std::get_if<memory_operand>(&o.get()); mem) {
   //                  auto base_register_idx = mem->base_register().index();
			//
			// 		if (!vreg_to_reg.count(base_register_idx)) {
   //                      auto allocation = regset.allocate(mem->base_register().type());
   //                      mem->base_register() = register_operand(allocation, mem->base_register().type());
   //                      // TODO: copy
   //                      util::global_logger.debug("Allocating virtual register to {}\n", o);
			// 		} else {
   //                      auto type = mem->base_register().type();
			// 			o.allocate_base(vreg_to_preg.at(vri), type);
			// 		}
			// 	}
			}
		}
	//
 //        if (has_unused_keep) {
 //            for (size_t i = 0; i < insn.operands().size(); i++) {
 //                const auto &op = insn.operands()[i];
 //                if (op.is_keep()) {
 //                    vreg_to_preg.erase(allocs[i].first);
 //                    avail_physregs.flip(allocs[i].second);
 //                }
 //            }
 //        }
	//
	// 	// Kill MOVs
 //        // TODO: refactor
 //        if (insn.opcode().find("mov") != std::string::npos) {
 //            operand op1 = insn.operands()[0];
 //            operand op2 = insn.operands()[1];
	//
 //            if (op1.is_preg() && op2.is_preg()) {
 //                if (op1.preg().register_index() == op2.preg().register_index()) {
 //                    insn.kill();
 //                }
 //            }
 //        }
	// }
}

