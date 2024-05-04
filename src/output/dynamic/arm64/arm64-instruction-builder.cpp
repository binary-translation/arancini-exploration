#include <arancini/output/dynamic/arm64/arm64-instruction-builder.h>

#include <arancini/output/dynamic/arm64/arm64-instruction.h>
#include <arancini/output/dynamic/arm64/arm64-translation-context.h>

#include <array>
#include <bitset>
#include <utility>
#include <type_traits>
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
            auto has_virtual = std::visit([](auto &&op)
            {
                using T = std::decay_t<decltype(op)>;
                if constexpr (std::is_same_v<T, register_operand>) {
                    return op.is_virtual();
                }
                if constexpr (std::is_same_v<T, memory_operand>) {
                    return op.base_register().is_virtual();
                }
                return false;
            }, op.get());

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

class register_set {
public:
    register_set(std::uint32_t available_scalar_registers, std::uint32_t available_neon_registers):
        scalar_registers_(available_scalar_registers),
        neon_registers_(available_neon_registers)
    { }

    using base_type = std::bitset<register_operand::physical_register_count>;

    register_operand::register_index allocate(const register_operand::register_type t) {
        auto &base = allocation_base(t);

        auto index = base._Find_first();
        base.flip(index);

        return index;
    }

    inline void deallocate(register_operand reg) {
        allocation_base(reg.type()).flip(reg.index());
    }
private:
    std::bitset<register_operand::physical_register_count> scalar_registers_;
    std::bitset<register_operand::physical_register_count> neon_registers_;
    std::bitset<register_operand::physical_register_count>& float_registers_ = neon_registers_;

    std::bitset<register_operand::physical_register_count> &allocation_base(arancini::ir::value_type t) {
        if (t.type_class() == arancini::ir::value_type_class::signed_integer ||
            t.type_class() == arancini::ir::value_type_class::unsigned_integer)
            return scalar_registers_;

        return float_registers_;
    }
};

class allocation_manager {
public:
    allocation_manager(register_set regset): regset_(regset)
    { }

    // Modifiers
    register_operand allocate(const register_operand &vreg) {
        auto allocation = register_operand{regset_.allocate(vreg.type()), vreg.type()};

        current_allocations_.emplace(vreg, allocation);
        current_allocations_.emplace(allocation, vreg);

        return allocation;
    }

    inline void deallocate(register_operand reg) {
        current_allocations_.erase(current_allocations_.at(reg));
        current_allocations_.erase(reg);

        regset_.deallocate(reg);
    }

    // Lookup
    const register_operand *get_allocation(const register_operand &vreg) {
        if (current_allocations_.count(vreg))
            return &current_allocations_.at(vreg);
        return nullptr;
    }
private:
    register_set regset_;

    struct register_type_hash {
        using value_type = register_operand::register_type;
        std::size_t operator()(const value_type &t) const {
            using type_class = std::underlying_type_t<value_type::value_type_class>;

            return std::hash<value_type::size_type>{}(t.nr_elements()) ^
                   std::hash<type_class>{}(static_cast<type_class>(t.type_class()));
        }
    };

    struct register_hash {
        std::size_t operator()(const register_operand &reg) const {
            return std::hash<register_operand::register_index>{}(reg.index()) ^
                   register_type_hash{}(reg.type());
        }
    };

    using register_map = std::unordered_map<register_operand, register_operand, register_hash>;
    register_map current_allocations_;
};

struct eliminate_moves {
    bool operator()(const register_operand &op1, const register_operand &op2) const {
        return op1 == op2;
    }

    // Fallback
    template <typename T1, typename T2>
    bool operator()(const T1 &op1, const T2 &op2) const {
        return false;
    }
};

// Reverse Linear Scan Allocation
//
// Procedure:
// 1. Iterate in reverse over the instruction stream with virtual registers
//
// -- def() allocation
// 2. For each def() (a virtual register), find a previous allocation to corresponds to a use()
// 3. If a previous allocation exists; allocate it to the same physical register
// 4. If no previous allocation exists; the def() has no corresponding use() later in the
// instruction stream and the instruction can be eliminated.
// 5. If the instruction is set as keep(), it must be kept and allocated to a new physical register
//
// -- use() and usedef() allocation: only get to this stage if the instruction has not been
// eliminated
// 6. For each use() virtual register, find a previous allocation
// 7. If a previous allocation exists, allocate to the same physical register
// 8. If no previous allocation exists, allocate to new physical register
// 9. Do the same for memory operands
//
// -- keep()
// 10. If instruction is defined as keep() and not used, find def() operand and free allocation
// 11. Optimizations: remove unused mov instructions
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
    allocation_manager allocator {register_set{0x1FBFFFFF, 0xFFFFFFFF}};

	for (auto RI = instructions_.rbegin(), RE = instructions_.rend(); RI != RE; RI++) {
		auto &insn = *RI;

        bool unused_keep = false;

        util::global_logger.debug("Allocating instruction {}\n", insn);
		for (auto &o : insn.operands()) {
            if (o.is_def() && !o.is_use()) {
                // Only regs can be definitions
                if (const auto *vreg = std::get_if<register_operand>(&o.get()); vreg && vreg->is_virtual()) {
                    util::global_logger.debug("Defining value {}\n", o);

                    if (const auto *previous_allocation = allocator.get_allocation(*vreg);
                        previous_allocation)
                    {
                        // Assign operand to previously allocated register
                        o = *previous_allocation;

                        util::global_logger.debug("Assign definition {} to existing allocation {}\n",
                                                  o, *previous_allocation);

                        // Allocation not needed beyond this point
                        allocator.deallocate(*previous_allocation);

                        util::global_logger.debug("Register {} available\n", *previous_allocation);
                    } else if (insn.is_keep()) {
                        // No previous allocation: no users of this definition exist
                        // Need to allocate anyway; since instruction is marked as keep()
                        util::global_logger.debug("No previous allocation exists for 'keep' definition {}\n", o);

                        auto allocation = allocator.allocate(*vreg);
                        o = allocation;

                        // Marked as unused
                        // Allocation becomes available before next instruction
                        unused_keep = true;

                        util::global_logger.debug("Unused definition allocated to {}\n", allocation);
                    } else {
                        // Instruction fully unused; eliminated
                        util::global_logger.debug("Value not allocated - killing instruction {}\n", insn);
                        insn.kill();
                        break;
                    }
                }
            } else {
                if (const auto *vreg = std::get_if<register_operand>(&o.get()); vreg && vreg->is_virtual()) {
                    if (const auto *previous_allocation = allocator.get_allocation(*vreg);
                        previous_allocation)
                    {
                        // Assign operand to previously allocated register
                        o = *previous_allocation;

                        util::global_logger.debug("Assign reference {} to existing allocation {}\n",
                                                  o, *previous_allocation);
                    } else {
                        auto allocation = allocator.allocate(*vreg);
                        o = allocation;

                        util::global_logger.debug("Assign reference {} to existing allocation {}\n",
                                                  o, allocation);
                    }
                }

                if (const auto *mem = std::get_if<memory_operand>(&o.get()); mem && mem->base_register().is_virtual()) {
                    const auto &base_vreg = mem->base_register();
                    if (const auto *previous_allocation = allocator.get_allocation(base_vreg);
                        previous_allocation)
                    {
                        // Assign operand to previously allocated register
                        o = memory_operand(*previous_allocation, mem->offset(), mem->addressing_mode());

                        util::global_logger.debug("Assign reference {} to existing allocation {}\n",
                                                  o, *previous_allocation);
                    } else {
                        auto allocation = allocator.allocate(base_vreg);
                        o = memory_operand(allocation, mem->offset(), mem->addressing_mode());

                        util::global_logger.debug("Assign reference {} to existing allocation {}\n",
                                                  o, allocation);
                    }
                }
            }
        }

        if (unused_keep) {
            for (auto &o : insn.operands()) {
                if (const auto *reg = std::get_if<register_operand>(&o.get()); o.is_def() && reg) {
                    allocator.deallocate(*reg);
                }
            }
        }

		// Kill MOVs
        if (insn.is_copy()) {
            auto op1 = insn.operands()[0].get();
            auto op2 = insn.operands()[1].get();
            if (std::visit(eliminate_moves{}, op1, op2))
                insn.kill();
        }
	}
}

