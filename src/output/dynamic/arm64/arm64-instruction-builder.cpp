#include <arancini/output/dynamic/arm64/arm64-instruction.h>
#include <arancini/output/dynamic/arm64/arm64-instruction-builder.h>

#include <bitset>
#include <unordered_map>

using namespace arancini::output::dynamic::arm64;

void instruction_builder::spill() {
}

[[nodiscard]]
inline bool is_virtual(const operand& op) {
    if (auto* reg = std::get_if<register_operand>(&op.get()); reg)
        return reg->is_virtual();
    if (auto* mem = std::get_if<memory_operand>(&op.get()); mem)
        return mem->is_virtual();
    return false;
}

void instruction_builder::emit(machine_code_writer &writer) {
    for (std::size_t i = 0; i < instructions_.size(); ++i) {
        const auto &instr = instructions_[i];

        if (instr.is_dead())
            continue;

        const auto &operands = instr.operands();

        for (std::size_t i = 0; i < instr.operand_count(); ++i) {
            const auto &op = operands[i];
            if (std::holds_alternative<std::monostate>(op.get()) || is_virtual(op)) {
                throw backend_exception("Virtual register after register allocation: {}", instr);
            }
        }
    }

    auto instruction_stream = fmt::format("{}", fmt::join(instruction_begin(), instruction_end(), "\n"));

    std::size_t size;
    std::uint8_t* encode;
    size = asm_.assemble(instruction_stream.c_str(), &encode);

    logger.debug("Translation:\n{}\n", instruction_stream);

    // TODO: write directly
    writer.copy_in(encode, size);

    // TODO: do zero-copy keystone
    asm_.free(encode);
}

template <std::size_t Size>
class register_set {
public:
    using value_type = std::bitset<Size>;

    register_set(value_type available_registers):
        registers_(available_registers)
    { }

    [[nodiscard]]
    std::size_t allocate(arancini::ir::value_type t) {
		// The reverse linear register reg_alloc we use expects that we'll find the first free
            // register, even when there exists a gap between "assigned" and "unassigned registers"
		auto index = registers_._Find_first();
        registers_.flip(index);
        return index;
    }

    void deallocate(std::size_t idx) {
        // Check if allocated previously
        [[unlikely]]
        if (registers_[idx] == 1)
            throw backend_exception("Cannot deallocate unallocated register");
        registers_.flip(idx);
    }
private:
    value_type registers_;
};

class system_register_set {
public:
    system_register_set(register_set<32> available_gp_regs, register_set<32> available_fp_regs):
        gpr_(available_gp_regs),
        fpr_(available_fp_regs)
    { }

    [[nodiscard]]
    std::size_t allocate(const arancini::ir::value_type t) {
        auto &base = allocation_base(t);
        return base.allocate(t);
    }

    inline void deallocate(register_operand reg) {
        allocation_base(reg.type()).deallocate(reg.index());
    }

    [[nodiscard]]
    register_set<32> gpr_state() const noexcept { return gpr_; }

    [[nodiscard]]
    register_set<32> fpr_state() const noexcept { return fpr_; }
private:
    register_set<32> gpr_;
    register_set<32> fpr_;

    register_set<32> &allocation_base(arancini::ir::value_type t) {
        if (t.type_class() == arancini::ir::value_type_class::signed_integer ||
            t.type_class() == arancini::ir::value_type_class::unsigned_integer)
            return gpr_;

        return fpr_;
    }
};

class register_allocator {
public:
    register_allocator(system_register_set regset):
        regset_(regset)
    { }

    // Modifiers
    [[nodiscard]]
    register_operand allocate(const register_operand &vreg) {
        auto allocation = register_operand{regset_.allocate(vreg.type()), vreg.type()};

        current_allocations_.emplace(vreg, allocation);
        current_allocations_.emplace(allocation, vreg);

        return allocation;
    }

    void deallocate(register_operand reg) {
        // Do nothing when register not tracked by register reg_alloc
        // TODO: check register reg_alloc behaviour when intermixing hardcoded def() physical registers
        // with use() virtual registers
        if (current_allocations_.find(reg) == current_allocations_.end()) return;

        current_allocations_.erase(current_allocations_.at(reg));
        current_allocations_.erase(reg);

        regset_.deallocate(reg);
    }

    // Lookup
    [[nodiscard]]
    const register_operand *get_allocation(const register_operand &vreg) {
        if (current_allocations_.count(vreg))
            return &current_allocations_.at(vreg);
        return nullptr;
    }

    [[nodiscard]]
    system_register_set state() const noexcept { return regset_; }
private:
    system_register_set regset_;

    struct register_type_hash {
        using value_type = arancini::ir::value_type;
        std::size_t operator()(const value_type &t) const {
            using type_class = std::underlying_type_t<arancini::ir::value_type_class>;

            return std::hash<value_type::size_type>{}(t.nr_elements()) ^
                   std::hash<type_class>{}(static_cast<type_class>(t.type_class()));
        }
    };

    struct register_hash {
        std::size_t operator()(const register_operand &reg) const {
            return std::hash<std::size_t>{}(reg.index()) ^
                   register_type_hash{}(reg.type());
        }
    };

    using register_map = std::unordered_map<register_operand, register_operand, register_hash>;
    register_map current_allocations_;
};

void instruction_builder::allocate() {
	// reverse linear scan reg_alloc
    // TODO: handle direct physical register usage
    // TODO: handle different sizes

    // All registers can be used except:
    // SP/zero - ARM64 limitation (x31),
    // Return to trampoline (x30)
    // Frame Pointer - points to guest registers (x29)
    system_register_set regset{register_set<32>{0x11FFFFFFF}, register_set<32>{0xFFFFFFFFF}};
    register_allocator reg_alloc{regset};

	for (auto it = instructions_.rbegin(); it != instructions_.rend(); it++) {
		auto &instr = *it;
        bool has_unused_keep = false;

        logger.debug("Allocating instruction {}\n", instr);


		// kill defs first
        for (auto& op : instr.operands()) {
			// Only regs can be /real/ defs
            if (op.is_def()) {
                // TODO: can't get() be avoided here?
                if (auto* vreg = std::get_if<register_operand>(&op.get()); vreg && vreg->is_virtual()) {
                    logger.debug("Defining register {}\n", *vreg);

                    // Get previous allocations
                    if (const auto* prev = reg_alloc.get_allocation(*vreg); prev) {
                        logger.debug("Assign definition {} to existing allocation {}\n",
                                     op, *prev);

                        // Assign operand to previously allocated register
                        op = *prev;

                        // Allocation not needed beyond this point
                        reg_alloc.deallocate(*prev);

                        logger.debug("Register {} available\n", *prev);
                    }
                } else if (instr.is_keep()) {
                    // No previous allocation: no users of this definition exist
                    // Need to allocate anyway; since instruction is marked as keep()
                    logger.debug("No previous allocation exists for 'keep' definition {}\n", op);

                    auto allocation = reg_alloc.allocate(*vreg);

                    // Marked as unused
                    // Allocation becomes available before next instruction
                    has_unused_keep = true;

                    logger.debug("Unused definition allocated to {}\n", allocation);

                    op = allocation;
                } else {
                    logger.debug("Register not allocated - killing instruction\n", op);
                    instr.kill();
                    break;
                }
            } else {
                if (const auto *vreg = std::get_if<register_operand>(&op.get()); vreg && vreg->is_virtual()) {
                    if (const auto *prev = reg_alloc.get_allocation(*vreg); prev) {
                        logger.debug("Assign reference {} to existing allocation {}\n", op, *prev);

                        op = *prev;
                    } else {
                        auto allocation = reg_alloc.allocate(*vreg);

                        logger.debug("Assign reference {} to new allocation {}\n", op, allocation);
                        op = allocation;
                    }
                } else if (const auto *mem = std::get_if<memory_operand>(&op.get());
                           mem && mem->base_register().is_virtual())
                {
                        const auto &base_vreg = mem->base_register();
                        if (const auto *prev = reg_alloc.get_allocation(base_vreg); prev) {
                            logger.debug("Assign reference {} to existing allocation {}\n", op, *prev);

                            op = memory_operand(*prev, mem->offset(), mem->mode());
                        } else {
                            auto allocation = reg_alloc.allocate(base_vreg);

                            logger.debug("Assign reference {} to existing allocation {}\n", op, allocation);

                            op = memory_operand(allocation, mem->offset(), mem->mode());
                        }
                }
            }
        }

        if (has_unused_keep) {
			logger.debug("Instruction {} is marked as keep but not referenced below; deallocating operands\n", instr);

			for (auto &op : instr.operands()) {
                if (!op.is_def()) continue;
                if (const auto *reg = std::get_if<register_operand>(&op.get()); reg) {
                    logger.debug("Deallocating register {}\n", *reg);
                    reg_alloc.deallocate(*reg);
                }
            }
        }

        // Kill MOVs
        // TODO: refactor
        if (instr.opcode().find("mov") != std::string::npos) {
            logger.debug("Attempting to eliminate copies between the same register\n");

            operand op1 = instr.operands()[0];
            operand op2 = instr.operands()[1];

            if (auto* reg1 = std::get_if<register_operand>(&op1.get()), *reg2 = std::get_if<register_operand>(&op2.get());
                    reg1 && reg2)
            {
                if (reg1->index() == reg2->index()) {
                    logger.debug("Killing instruction {} as part of copy optimization\n", instr);

                    instr.kill();
                }
            }
        }
    }
}

