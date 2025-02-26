#include <arancini/output/dynamic/arm64/arm64-instruction.h>
#include <arancini/output/dynamic/arm64/arm64-instruction-builder.h>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <bitset>
#include <unordered_map>
#include <unordered_set>

using namespace arancini::output::dynamic::arm64;
using value_type = arancini::ir::value_type;

std::size_t assembler::assemble(const char *code, unsigned char **out) {
    std::size_t size = 0;
    std::size_t count = 0;
    int status = ks_asm(ks_, code, 0, out, &size, &count);

    [[unlikely]]
    if (status == -1)
        throw backend_exception("Keystone assembler encountered invalid instruction error");

    [[unlikely]]
    if (status != 0)
        throw backend_exception("Keystone assembler encountered error after {} instructions: {}",
                                count, ks_strerror(ks_errno(ks_)));

    return size;
}

[[nodiscard]]
inline std::size_t get_min_bitsize(unsigned long long imm) {
    return value_types::base_type.element_width() - __builtin_clzll(imm|1);
}

value virtual_register_allocator::allocate(ir::value_type type) {
    std::size_t reg_count = type.nr_elements();
    auto element_width = type.width();
    if (element_width > value_types::base_type.width()) {
        element_width = value_types::base_type.width();
        reg_count = type.width() / element_width;
    }

    auto reg_type = ir::value_type(type.type_class(), element_width, 1);

    register_sequence regset;
    for (std::size_t i = 0; i < reg_count; ++i) {
        auto reg = register_operand(next_vreg_++, reg_type);
        regset.push_back(reg);
    }

    return value(regset, type);
}

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

    logger.debug("Translation (after register allocation):\n{}\n", instruction_stream);

    // TODO: write directly
    writer.copy_in(encode, size);

    // TODO: do zero-copy keystone
    asm_.free(encode);
}

template <std::size_t Size>
class register_set final {
public:
    using value_type = std::bitset<Size>;

    register_set(value_type available_registers):
        registers_(available_registers)
    { }

    [[nodiscard]]
    std::size_t allocate(arancini::ir::value_type t) {
		// The reverse linear register reg_alloc we use expects that we'll find the first free
        // register, even when there exists a gap between "assigned" and "unassigned registers"
		auto idx = registers_._Find_first();

        [[unlikely]]
        if (idx >= registers_.size()) {
            throw backend_exception("run out registers to allocate and register spilling not supported");
        }

        registers_.flip(idx);
        return idx;
    }

    void deallocate(std::size_t idx) {
        [[unlikely]]
        if (idx >= registers_.size()) {
            throw backend_exception("attempting to deallocate register index {} but only {} registers exist",
                                    idx, registers_.size());
        }

        // Check if allocated previously
        [[unlikely]]
        if (registers_[idx] == 1)
            throw backend_exception("Cannot deallocate unallocated register");
        registers_.flip(idx);
    }

    value_type registers() const { return registers_; }
private:
    value_type registers_;
};

template <std::size_t Size>
bool operator==(const register_set<Size>& rset1, const register_set<Size>& rset2) {
    return rset1.registers() == rset2.registers();
}

template <std::size_t Size>
bool operator!=(const register_set<Size>& rset1, const register_set<Size>& rset2) {
    return !(rset1 == rset2);
}

template <std::size_t N>
struct fmt::formatter<std::bitset<N>> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const std::bitset<N>& bitset, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", bitset.to_string());
    }
};

template <std::size_t N>
struct fmt::formatter<register_set<N>> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const register_set<N>& regset, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", regset.registers());
    }
};

class system_register_set final {
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

template <>
struct fmt::formatter<system_register_set> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const system_register_set& regset, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "GPR: {}, FPR: {}", regset.gpr_state(), regset.fpr_state());
    }
};

bool operator==(const system_register_set& rset1, const system_register_set& rset2) {
    return rset1.gpr_state() == rset2.gpr_state() && rset1.fpr_state() == rset2.fpr_state();
}

bool operator!=(const system_register_set& rset1, const system_register_set& rset2) {
    return !(rset1 == rset2);
}

struct register_hash final {
    std::size_t operator()(const register_operand &reg) const {
        return std::hash<std::size_t>{}(reg.index());
    }
};

struct register_equal final {
    bool operator()(const register_operand& r1, const register_operand& r2) const {
        return r1.index() == r2.index() && r1.type().type_class() == r2.type().type_class();
    }
};

class physical_register_allocator final {
public:
    using register_map = std::unordered_map<register_operand, register_operand, register_hash, register_equal>;

    physical_register_allocator(system_register_set regset):
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

    void deallocate(const register_operand& preg) {
        // Do nothing when register not tracked by register reg_alloc
        // TODO: check register reg_alloc behaviour when intermixing hardcoded def() physical registers
        // with use() virtual registers
        auto& vreg = current_allocations_.at(preg);
        current_allocations_.erase(vreg);
        current_allocations_.erase(preg);

        regset_.deallocate(preg);
    }

    // Lookup
    [[nodiscard]]
    const register_operand *get_allocation(const register_operand &vreg) {
        if (current_allocations_.count(vreg)) {
            auto reg = &current_allocations_.at(vreg);
            reg->cast(vreg.type());
            return reg;
        }
        return nullptr;
    }

    [[nodiscard]]
    system_register_set state() const noexcept { return regset_; }

    using iterator = register_map::iterator;
    using const_iterator = register_map::const_iterator;

    iterator begin() { return current_allocations_.begin(); }
    const_iterator begin() const { return current_allocations_.begin(); }
    const_iterator cbegin() const { return current_allocations_.cbegin(); }

    iterator end() { return current_allocations_.end(); }
    const_iterator end() const { return current_allocations_.end(); }
    const_iterator cend() const { return current_allocations_.cend(); }
private:
    system_register_set regset_;
    register_map current_allocations_;
};

bool fulfills_keep(const instruction& instr,
                   const register_operand& op,
                   const std::unordered_set<register_operand, register_hash>& implicit_deps)
{
    return instr.is_keep() || implicit_deps.count(op);
}

class implicit_dependency_handler final {
public:
    void satisfy(const register_operand& op) {
        logger.debug("Implicit dependency on {} satisfied by write\n", op);
        deps_.erase(op);
    }

    void insert(const std::vector<register_operand>& implicit_dependencies) {
        deps_.insert(implicit_dependencies.begin(),
                     implicit_dependencies.end());
    }

    bool fulfills(const register_operand& reg) {
        return deps_.count(reg) != 0;
    }

    bool empty() const { return deps_.empty(); }
private:
    std::unordered_set<register_operand, register_hash> deps_;
};

class branch_liveness_tracker final {
public:
    void track_label(const instruction& instr, const std::unordered_map<std::string, std::size_t>& label_refcount) {
        if (instr.label().empty()) return;

        const std::string& label_name = instr.label().name();

        // We've already seen this label referrenced by a later branch
        // So, this is a label for a backwards branch
        logger.debug("Found branch label {}\n", label_name);
        if (expected_labels_.count(label_name)) {
            expected_labels_.erase(label_name);
            logger.debug("Branch label {} matches to backward branch; {} remaining backward branches\n",
                         label_name, expected_labels_.size());
            return;
        }

        // First time we see label
        // Branches referencing it are higher in the instruction stream (forward branches)
        tracker_[label_name] = label_refcount.at(label_name);
        logger.debug("Found label {} for forward branch; tracking {} forward branches\n",
                     label_name, tracker_.size());
    }

    void track_branch(const instruction& branch) {
        [[unlikely]]
        if (!branch.is_branch())
            throw backend_exception("attempting to track non-branch as branch instruction: {}", branch);

        // Extract branch target
        const label_operand* label_op = nullptr;
        for (const auto& op : branch.operands()) {
            label_op = std::get_if<label_operand>(&op.get());
            if (label_op) {
                break;
            }
        }

        auto label_name = label_op->name();

        // If branch target is tracked, it must be have been seen
        // This means that we branch forward in the instruction stream (since we allocate registers
        // in reverse)
        if (tracker_.count(label_name)) {
            logger.debug("Found forward branch to {}; remaining {}\n",
                         label_op->name(), tracker_[label_name]-1);
            if (--tracker_[label_name] == 0)
                tracker_.erase(label_name);
            return;
        }

        // If branch target is not tracked, we must be jumping backwards to some unseen label (as of
        // now)
        expected_labels_.insert(label_name);
        logger.debug("Found backward branch to {}; tracked backward branches {}\n",
                     label_op->name(), expected_labels_.size());
    }

    [[nodiscard]]
    bool in_branch_block() const { return !(tracker_.empty() && expected_labels_.empty()); }
private:
    std::unordered_map<std::string, std::size_t> tracker_;
    std::unordered_set<std::string> expected_labels_;
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
    physical_register_allocator reg_alloc{regset};

    branch_liveness_tracker branch_tracker;
    implicit_dependency_handler implicit_dependencies;

    auto instruction_stream = fmt::format("{}", fmt::join(instruction_begin(), instruction_end(), "\n"));
    logger.debug("Translation (before register allocation):\n{}\n", instruction_stream);

    bool in_branch_block = false;
	for (auto rit = instructions_.rbegin(); rit != instructions_.rend(); rit++) {
		auto &instr = *rit;
        bool has_unused_keep = false;

        logger.debug("Allocating instruction {}\n", instr);

        implicit_dependencies.insert(instr.implicit_dependencies());

        // Handle side-effects
        for (const auto& reg_side_effect : instr.side_effect_writes()) {
            if (implicit_dependencies.fulfills(reg_side_effect)) {
                instr.as_keep();
                implicit_dependencies.satisfy(reg_side_effect);
            }
        }

        branch_tracker.track_label(instr, label_refcount_);

        // If we just exited a branch block
        if (in_branch_block && !branch_tracker.in_branch_block()) {
            logger.debug("Exited branch block; eliminating unassigned loop mappings\n");

            std::unordered_map<register_operand, std::size_t, register_hash> defs;
            for (const auto& [reg1, reg2] : reg_alloc) {
                if (reg1.is_virtual())
                    defs[reg1] = 0;
            }

            for (auto it = instructions_.begin(); it != rit.base(); ++it) {
                for (const auto& op : it->operands()) {
                    if (!op.is_def()) continue;
                    auto& reg = std::get<register_operand>(op.get());
                    if (reg.is_virtual()) // TODO: should consider physical
                        defs[reg] += 1;
                }
            }

            // Go over defined regs and eliminate those that are not referrenced
            for (const auto& [vreg, def_count] : defs) {
                if (!def_count) {
                    logger.debug("Found undefined virtual register: {}\n", vreg);
                    auto preg = reg_alloc.get_allocation(vreg);
                    if (preg) {
                        logger.debug("Deallocating unassigned loop mapping {} -> {}\n", vreg, *preg);
                        reg_alloc.deallocate(*preg);
                    }
                } else {
                    logger.debug("Keeping virtual register {} allocated because it has {} def counts\n",
                                 vreg, def_count);
                }
            }
        }
        in_branch_block = branch_tracker.in_branch_block();

        logger.debug("Current allocation state {}\n", reg_alloc.state());

        for (auto& op : instr.operands()) {
            [[unlikely]]
            if (!op.is_def()) continue;

            // kill defs first
            // Only registers can be defs
            // TODO: can't get() be avoided here?
            auto vreg = std::get<register_operand>(op.get());
            logger.debug("Defining register {}\n", vreg);

            if (!vreg.is_virtual() && implicit_dependencies.fulfills(vreg)) {
                instr.as_keep();
                implicit_dependencies.satisfy(vreg);
                continue;
            }

            logger.debug("Current state\n");
            for (const auto& [reg1, reg2] : reg_alloc) {
                logger.debug("{} -> {}\n", reg1, reg2);
            }

            if (!vreg.is_virtual()) continue;

            // Get previous allocations
            if (const auto* prev = reg_alloc.get_allocation(vreg); prev) {
                logger.debug("Assign definition {} to existing allocation {}\n",
                             op, *prev);

                bool is_use = op.is_use();

                // Assign operand to previously allocated register
                op = *prev;
                op.as_def();

                // If there existed any implicit dependency on the allocated register, we satisfy it
                implicit_dependencies.satisfy(*prev);

                // Allocation not needed beyond this point
                // TODO: enable this when backwards branches are working
                if (!branch_tracker.in_branch_block() && !is_use) {
                    reg_alloc.deallocate(*prev);
                    logger.debug("Register {} available\n", *prev);
                }
            } else if (instr.is_keep() || implicit_dependencies.fulfills(op) || branch_tracker.in_branch_block()) {
                // No previous allocation: no users of this definition exist
                // Need to allocate anyway; since instruction is marked as keep()
                logger.debug("No previous allocation exists for 'keep' definition {}\n", op);

                auto allocation = reg_alloc.allocate(vreg);

                // Marked as unused
                // Allocation becomes available before next instruction
                has_unused_keep = true;

                logger.debug("Unused definition allocated to {}\n", allocation);

                op = allocation;
                op.as_def();
            } else {
                logger.debug("Register not allocated - killing instruction\n", op);
                instr.kill();
            }

            break;
        }

        if (instr.is_dead()) continue;

        for (auto& op : instr.operands()) {
            [[unlikely]]
            if (!op.is_use()) continue;

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

        if (has_unused_keep) {
			logger.debug("Instruction {} is marked as keep but not referenced below; deallocating definitions\n", instr);

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
        if (instr.opcode().find("mov") != std::string::npos && !instr.is_keep()) {
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

        if (instr.is_branch())
            branch_tracker.track_branch(instr);
    }

    // Check that no dangling allocations remained
    [[unlikely]]
    if (reg_alloc.state() != regset)
        throw backend_exception("Dangling allocations after register allocation:\n{} != {} (ref != actual)",
                                reg_alloc.state(), regset);

    [[unlikely]]
    if (branch_tracker.in_branch_block())
        throw backend_exception("Register allocation detected incomplete branch block");

    // [[unlikely]]
    // if (implicit_dependencies.empty())
    //     throw backend_exception("Register allocation detected unsatisfied implicit dependencies {}",
    //                             implicit_dependencies);

}

