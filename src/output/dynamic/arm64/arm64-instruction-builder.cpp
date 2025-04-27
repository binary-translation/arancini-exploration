#include <arancini/output/dynamic/arm64/arm64-instruction-builder.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <array>
#include <bitset>
#include <unordered_map>
#include <utility>

using namespace arancini::output::dynamic::arm64;

void instruction_builder::spill() {}

[[nodiscard]]
inline bool is_virtual(const operand &op) {
    if (auto *reg = std::get_if<register_operand>(&op.get()); reg)
        return reg->is_virtual();
    if (auto *mem = std::get_if<memory_operand>(&op.get()); mem)
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
            if (std::holds_alternative<std::monostate>(op.get()) ||
                is_virtual(op)) {
                throw backend_exception(
                    "Virtual register after register allocation: {}", instr);
            }
        }
    }

    auto instruction_stream = fmt::format(
        "{}", fmt::join(instruction_begin(), instruction_end(), "\n"));

    std::size_t size;
    std::uint8_t *encode;
    size = asm_.assemble(instruction_stream.c_str(), &encode);

    logger.debug("Translation:\n{}\n", instruction_stream);

    // TODO: write directly
    writer.copy_in(encode, size);

    // TODO: do zero-copy keystone
    asm_.free(encode);
}

void instruction_builder::allocate() {
    // reverse linear scan allocator
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

    for (auto RI = instructions_.rbegin(), RE = instructions_.rend(); RI != RE;
         RI++) {
        auto &instr = *RI;

        logger.debug("Allocating instruction {}\n", instr);

        std::array<std::pair<size_t, size_t>, 5> allocs;
        bool has_unused_keep = false;

        auto allocate = [&allocs, &avail_physregs, &avail_float_physregs,
                         &vreg_to_preg](operand &o, size_t idx) -> void {
            unsigned int vri;
            ir::value_type type;
            if (auto *reg = std::get_if<register_operand>(&o.get());
                reg && reg->is_virtual()) {
                vri = reg->index();
                type = reg->type();
            } else if (auto *mem = std::get_if<memory_operand>(&o.get());
                       mem && mem->is_virtual()) {
                auto vreg = mem->base_register();
                vri = vreg.index();
                type = vreg.type();
            } else {
                throw backend_exception(
                    "Trying to allocate non-virtual register operand");
            }

            std::size_t allocation = 0;
            if (type.is_floating_point()) {
                allocation = avail_float_physregs._Find_first();
                avail_float_physregs.flip(allocation);
            } else {
                allocation = avail_physregs._Find_first();
                avail_physregs.flip(allocation);
            }

            // TODO: register spilling
            vreg_to_preg[vri] = allocation;

            if (std::holds_alternative<memory_operand>(o.get()))
                o.allocate_base(allocation, type);
            else
                o.allocate(allocation, type);

            allocs[idx] = std::make_pair(vri, allocation);
        };

        // kill defs first
        for (std::size_t i = 0; i < instr.operand_count(); i++) {
            auto &o = instr.operands()[i];

            // Only regs can be /real/ defs
            if (auto *reg = std::get_if<register_operand>(&o.get());
                o.is_def() && !o.is_use() && reg && reg->is_virtual()) {
                logger.debug("Defining register {}\n", o);

                auto type = reg->type();
                unsigned int vri = reg->index();

                auto alloc = vreg_to_preg.find(vri);

                if (alloc != vreg_to_preg.end()) {
                    int pri = alloc->second;

                    if (type.is_floating_point()) {
                        avail_float_physregs.set(pri);
                    } else {
                        avail_physregs.set(pri);
                    }

                    o.allocate(pri, type);

                    logger.debug("Allocated to {} -- releasing\n", o);
                } else if (instr.is_keep()) {
                    has_unused_keep = true;

                    allocate(o, i);
                } else {
                    logger.debug(
                        "Register not allocated - killing instruction\n", o);
                    instr.kill();
                    break;
                }
            }
        }

        if (instr.is_dead()) {
            continue;
        }

        // alloc uses next
        for (std::size_t i = 0; i < instr.operand_count(); i++) {
            auto &o = instr.operands()[i];

            // We only care about REG uses - but we also need to consider REGs
            // used in MEM expressions
            if (auto *reg = std::get_if<register_operand>(&o.get());
                o.is_use() && reg && reg->is_virtual()) {
                logger.debug("Allocating register {}\n", o);

                auto type = reg->type();
                unsigned int vri = reg->index();
                if (!vreg_to_preg.count(vri)) {
                    allocate(o, i);
                    logger.debug("Allocating register to {}\n", o);
                } else {
                    o.allocate(vreg_to_preg.at(vri), type);
                }

            } else if (auto *mem = std::get_if<memory_operand>(&o.get()); mem) {
                logger.debug("Allocating base register used as part of memory "
                             "reference {}\n",
                             o);

                if (mem->is_virtual()) {
                    unsigned int vri = mem->base_register().index();

                    if (!vreg_to_preg.count(vri)) {
                        allocate(o, i);
                        logger.debug("Allocating base register to {}\n", o);
                    } else {
                        auto type = mem->base_register().type();
                        o.allocate_base(vreg_to_preg.at(vri), type);
                    }
                }
            }
        }

        if (has_unused_keep) {
            for (std::size_t i = 0; i < instr.operand_count(); i++) {
                if (instr.operands()[i].is_def()) {
                    vreg_to_preg.erase(allocs[i].first);
                    avail_physregs.flip(allocs[i].second);
                }
            }
        }

        // Kill MOVs
        // TODO: refactor
        if (instr.opcode().find("mov") != std::string::npos) {
            logger.debug(
                "Attempting to eliminate copies between the same register\n");

            operand op1 = instr.operands()[0];
            operand op2 = instr.operands()[1];

            if (auto *reg1 = std::get_if<register_operand>(&op1.get()),
                *reg2 = std::get_if<register_operand>(&op2.get());
                reg1 && reg2) {
                if (reg1->index() == reg2->index()) {
                    logger.debug(
                        "Killing instruction {} as part of copy optimization\n",
                        instr);

                    instr.kill();
                }
            }
        }
    }
}
