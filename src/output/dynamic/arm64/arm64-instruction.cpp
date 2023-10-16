#include "arancini/ir/node.h"
#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <cstdint>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

using namespace arancini::output::dynamic::arm64;

std::string arancini::output::dynamic::arm64::to_string(const preg_operand &op) {
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

    static const char* name_special[] = {
        "",
        "nzcv"
    };

    // TODO: introduce check for NEON availability
    static const char* name_vector_neon[] = {
        "",
        "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10",
        "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20",
        "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30",
        "v31"
    };

    // TODO: introduce check for SVE2 availability
    static const char* name_vector_sve2[] = {
        "",
        "z0", "z1", "z2", "z3", "z4", "z5", "z6", "z7", "z8", "z9", "z10",
        "z11", "z12", "z13", "z14", "z15", "z16", "z17", "z18", "z19", "z20",
        "z21", "z22", "z23", "z24", "z25", "z26", "z27", "z28", "z29", "z30",
        "z31"
    };

    // TODO: vector predicates not used yet
    static const char* name_vector_pred[] = {
        "",
        "p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9", "p10",
        "p11", "p12", "p13", "p14", "p15"
    };

    auto type = op.type();
    size_t reg_idx = op.register_index();

    if (op.special())
        return name_special[reg_idx];

    if (type.is_vector()) {
        std::string name = type.width() == 128 ? name_vector_neon[reg_idx] : name_vector_sve2[reg_idx];
        switch (type.width()) {
        case 8:
            name += ".b";
            break;
        case 16:
            name += ".h";
            break;
        case 32:
            name += ".w";
            break;
        case 64:
            name += ".d";
            break;
        default:
            throw std::runtime_error("Vectors larger than 64-bit not supported");
        }
        return name;
    } else if (type.is_floating_point()) {
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
        std::string msg("Keystone assembler encountered error after count: ");
        msg += std::to_string(count);
        msg += ": ";
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

    if (!comment_.empty())
        os << " // " << comment_;
}

// TODO: adapt all
void operand::dump(std::ostream &os) const {
	switch (op_type()) {
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

        if (!memory().offset().value()) {
            os << "]";
            break;
        }

        if (!memory().post_index())
            os << ", #0x" << std::hex << memory().offset().value() << ']';
        else if (memory().pre_index())
            os << '!';

        if (memory().post_index())
            os << "], #0x" << std::hex << memory().offset().value();


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

