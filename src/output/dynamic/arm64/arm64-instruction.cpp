#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <cstdint>
#include <unordered_set>

using namespace arancini::output::dynamic::arm64;

const char* preg_operand::to_string() const {
    static const char* name64[] = {
        "",
        "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10",
        "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x19", "x20",
        "x21", "x22", "x23", "x24", "x25", "x26", "x27", "x28", "x29", "x30",
        "sp"
    };

    static const char* name32[] = {
        "",
        "w0", "w1", "w2", "w3", "w4", "w5", "w6", "w7", "w8", "w9", "w10",
        "w11", "w12", "w13", "w14", "w15", "w16", "w17", "w18", "w19", "w20",
        "w21", "w22", "w23", "w24", "w25", "w26", "w27", "w28", "w29", "w30",
        "sp"
    };

    uint8_t reg_idx = static_cast<uint8_t>(reg_);
    if (width() == 32)
        return name32[reg_idx];
    return name64[reg_idx];
}

size_t assembler::assemble(const char *code, unsigned char **out) {
    size_t size = 0;
    size_t count = 0;
    if (ks_asm(ks_, code, 0, out, &size, &count)) {
        std::string msg("Keystone assembler encountered error: ");
        throw std::runtime_error(msg + ks_strerror(ks_errno(ks_)));
    }

    return size;
}

void instruction::dump(std::ostream &os) const {
    os << opcode;

    if (opcount == 0) return;
	for (size_t i = 0; i < opcount - 1; ++i) {
        os << ' ';
        operands[i].dump(os);
        os << ',';
	}

    os << ' ';
    operands[opcount - 1].dump(os);
}

// TODO: adapt all
void operand::dump(std::ostream &os) const {
	switch (type) {
    case operand_type::cond:
        os << condition.cond;
        break;
    case operand_type::label:
        os << label.name;
        break;
    case operand_type::shift:
        if (!shift.modifier.empty())
            os << shift.modifier << ' ';
        os << "#0x" << std::hex << shift.u64;
        break;
	case operand_type::imm:
		os << "#0x" << std::hex << immediate.u64;
		break;

	case operand_type::mem:
		os << "[";

		if (memory.virt_base)
			os << "%V" << memory.vbase.width << "_" << std::dec << memory.vbase.index;
		else
			os << memory.pbase.to_string();


        if (!memory.post_index)
            os << ", #0x" << std::hex << memory.offset << ']';
        else if (memory.pre_index)
            os << '!';

        if (memory.post_index)
            os << "], #0x" << std::hex << memory.offset;


        // TODO: register indirect with index
		break;

	case operand_type::preg:
		os << preg.to_string();
		break;

	case operand_type::vreg:
		os << "%V" << std::dec << vreg.index;
		break;
    default:
        throw std::runtime_error("operand::dump() encountered invalid operand type: "
                                 + std::to_string(static_cast<unsigned>(type)));
	}
}

