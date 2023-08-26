#include "arancini/ir/node.h"
#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <cstdint>
#include <ostream>
#include <sstream>
#include <unordered_set>

using namespace arancini::output::dynamic::arm64;

const char* arancini::output::dynamic::arm64::to_string(const preg_operand &op) {
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

    static const char* name_float32[] = {
        "",
        "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10",
        "s11", "s12", "s13", "s14", "s15", "s16", "s17", "s18", "s19", "s20",
        "s21", "s22", "s23", "s24", "s25", "s26", "s27", "s28", "s29", "s30",
        "s31"
    };

    static const char* name_float64[] = {
        "",
        "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "d10",
        "d11", "d12", "d13", "d14", "d15", "d16", "d17", "d18", "d19", "d20",
        "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30",
        "d31"
    };

    auto type = op.type();
    size_t reg_idx = op.register_index();

    if (type.is_floating_point()) {
        return type.element_width() > 32 ? name_float64[reg_idx] : name_float32[reg_idx];
    } else if (type.is_integer()) {
        return type.element_width() > 32 ? name64[reg_idx] : name32[reg_idx];
    } else {
        throw std::runtime_error("Physical registers are specified as 32-bit or 64-bit only");
    }
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

std::string instruction::dump() const {
    std::ostringstream ss;
    dump(ss);
    return ss.str();
}

void instruction::dump(std::ostream &os) const {
    os << opcode_;

    if (opcount_ == 0) return;
	for (size_t i = 0; i < opcount_ - 1; ++i) {
        os << ' ';
        operands_[i].dump(os);
        os << ',';
	}

    os << ' ';
    operands_[opcount_ - 1].dump(os);
}

// TODO: adapt all
void operand::dump(std::ostream &os) const {
	switch (type()) {
    case operand_type::cond:
        // TODO: rename
        os << cond().condition();
        break;
    case operand_type::label:
        os << label().name();
        break;
    case operand_type::shift:
        if (!shift().modifier().empty())
            os << shift().modifier() << ' ';
        os << "#0x" << std::hex << shift().value();
        break;
	case operand_type::imm:
		os << "#0x" << std::hex << immediate().value();
		break;

	case operand_type::mem:
		os << "[";

		if (memory().is_virtual())
			os << "%V" << memory().vreg_base().width()
               << "_" << std::dec << memory().vreg_base().index();
		else
			os << to_string(memory().preg_base());


        if (!memory().post_index())
            os << ", #0x" << std::hex << memory().offset() << ']';
        else if (memory().pre_index())
            os << '!';

        if (memory().post_index())
            os << "], #0x" << std::hex << memory().offset();


        // TODO: register indirect with index
		break;

	case operand_type::preg:
		os << to_string(preg());
		break;

	case operand_type::vreg:
		os << "%V" << std::dec << vreg().index();
		break;
    default:
        throw std::runtime_error("operand::dump() encountered invalid operand type: "
                                 + std::to_string(op_.index()));
	}
}

