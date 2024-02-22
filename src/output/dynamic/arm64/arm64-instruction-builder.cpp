#include "arancini/ir/value-type.h"
#include <arancini/util/logger.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>
#include <arancini/output/dynamic/arm64/arm64-instruction-builder.h>
#include <array>
#include <bitset>
#include <exception>
#include <optional>
#include <sstream>
#include <utility>
#include <stdexcept>
#include <unordered_map>

using namespace arancini::output::dynamic::arm64;

static const vreg_operand *get_vreg_or_base(const operand &o) {
    if (o.is_vreg()) return &o.vreg();
    if (o.is_mem() && o.memory().is_virtual()) return &o.memory().vreg_base();
    else return nullptr;
}

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

        for (size_t i = 0; i < insn.operand_count(); ++i) {
            const auto &op = operands[i];
            if (op.op_type() == operand_type::invalid ||
                op.op_type() == operand_type::vreg ||
                (op.op_type() == operand_type::mem && op.memory().is_virtual())) {
                util::global_logger.error("Instruction builder failed:\n{}\n", *this);
                throw std::runtime_error("Virtual register after register allocation: "
                                         + fmt::format("{}", insn));
            }
        }
    }

    // TODO: change to use fmt
    auto assembly = fmt::format("{}", *this);

    size = asm_.assemble(assembly.c_str(), &encode);

    util::global_logger.info("Produced translation:\n{}\n", assembly);

    // TODO: write directly
    writer.copy_in(encode, size);

    // TODO: do zero-copy keystone
    asm_.free(encode);
}

class physical_registers {
public:
    physical_registers(size_t available_physical_registers_mask,
                       size_t available_float_physical_registers_mask):
        avail_physregs_(available_physical_registers_mask),
        avail_float_physregs_(available_float_physical_registers_mask)
    { }

    size_t allocate(const arancini::ir::value_type &type) {
        std::bitset<32> *mask;
        if (type.is_floating_point())
            mask = &avail_float_physregs_;
        else
            mask = &avail_physregs_;

        size_t allocation = mask->_Find_first();
        if (allocation >= mask->size())
            throw std::runtime_error("No registers available to allocate; register spilling required");

        mask->flip(allocation);

        return allocation;
    }

    size_t allocate(size_t index, const arancini::ir::value_type &type) {
        if (type.is_floating_point()) {
            avail_float_physregs_.flip(index);
        } else {
            avail_physregs_.flip(index);
        }

        return index;
    }

    size_t deallocate(size_t index, const arancini::ir::value_type &type) {
        if (type.is_floating_point()) avail_float_physregs_.flip(index);
        else avail_physregs_.flip(index);
        return index;
    }
private:
	std::bitset<32> avail_physregs_;
	std::bitset<32> avail_float_physregs_;
};

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
    physical_registers physregs(0x1FFBFFFFF, 0xFFFFFFFFF);

	for (auto RI = instructions_.rbegin(), RE = instructions_.rend(); RI != RE; RI++) {
		auto &insn = *RI;

        bool has_unused_keep = false;
        std::array<std::pair<size_t, size_t>, 5> allocs;

        auto allocate = [&allocs, &physregs, &vreg_to_preg] (operand &o, size_t idx) -> void
        {
                const vreg_operand *vreg = get_vreg_or_base(o);
                if (vreg == nullptr)
                    throw std::runtime_error("Trying to allocate non-virtual register operand");

                // TODO: register spilling
                size_t allocation = physregs.allocate(vreg->type());
                vreg_to_preg[vreg->index()] = allocation;

                o.allocate(allocation, vreg->type());
                allocs[idx] = std::make_pair(vreg->index(), allocation);
        };

		// kill defs first
		for (size_t i = 0; i < insn.operand_count(); i++) {
			auto &o = insn.operands()[i];

			// Only regs can be /real/ defs
			if (o.is_def() && o.is_vreg() && !o.is_use()) {
                auto type = o.vreg().type();
				unsigned int vri = o.vreg().index();

				auto alloc = vreg_to_preg.find(vri);
				if (alloc != vreg_to_preg.end()) {
					int pri = alloc->second;
                    physregs.allocate(pri, type);
					o.allocate(pri, type);

                    // No need to track this virtual register anymore, it is
                    // overwritten here by the def()
                    vreg_to_preg.erase(vri);
                    util::global_logger.debug("Allocated to {} -- releasing\n", o);
                } else if (o.is_keep()) {
                    has_unused_keep = true;

                    allocate(o, i);
				} else {
					insn.kill();
					break;
				}
			}
		}

        // Instruction has been eliminated; will not be allocated
		if (insn.is_dead()) continue;

		// alloc uses next
		for (size_t i = 0; i < insn.operand_count(); i++) {
			auto &o = insn.operands()[i];

			// We only care about REG uses - but we also need to consider REGs used in MEM expressions
            const vreg_operand *vreg = nullptr;
            if (o.is_use() && (vreg = get_vreg_or_base(o)) != nullptr) {
                util::global_logger.debug("Use {}", o);
                auto type = vreg->type();
                unsigned int vri = vreg->index();
				if (!vreg_to_preg.count(vri)) {
                    allocate(o, i);
				} else {
					o.allocate(vreg_to_preg.at(vri), type);
				}
            }
        }

        if (has_unused_keep) {
            for (size_t i = 0; i < insn.operand_count(); i++) {
                const auto &op = insn.operands()[i];
                if (op.is_keep()) {
                    vreg_to_preg.erase(allocs[i].first);
                    physregs.deallocate(allocs[i].second, op.type());
                }
            }
        }

		// Kill MOVs
        // TODO: refactor
        if (insn.is_copy()) {
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


constexpr fmt::format_parse_context::iterator fmt::formatter<instruction_builder>::parse(const fmt::format_parse_context &parse_ctx) {
    return parse_ctx.begin();
}

fmt::format_context::iterator fmt::formatter<instruction_builder>::format(const instruction_builder &builder, fmt::format_context &format_ctx) const {
    for (const auto &insn : builder.instructions()) {
        if (!insn.is_dead()) {
            fmt::format_to(format_ctx.out(), "{}\n", insn);
        }
    }
}

