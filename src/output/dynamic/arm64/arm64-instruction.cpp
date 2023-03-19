#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <cstdint>
#include <unordered_set>

using namespace arancini::output::dynamic::arm64;

const char* arm64_physreg_op::to_string() const {
    static const char* name[] = {
        "",
        "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10",
        "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x19", "x20",
        "x21", "x22", "x23", "x24", "x25", "x26", "x27", "x28", "x29", "x30",
        "xzr_sp"
    };

    return name[static_cast<size_t>(reg_)];
}

size_t assembler::assemble(const char *code, unsigned char **out) {
    size_t size = 0;
    if (ks_asm(ks_, code, 0, out, &size, nullptr)) {
        std::string msg("Keystone assembler encountered error: ");
        throw std::runtime_error(msg + ks_strerror(ks_errno(ks_)));
    }
    return size;
}

void arm64_instruction::emit(machine_code_writer &writer) const {
	if (opcode.empty()) {
		return;
	}

    size_t size;
    uint8_t* encode;
    std::stringstream assembly;

	switch (opform) {
	case arm64_opform::OF_NONE:
		break;

	case arm64_opform::OF_R32:
	case arm64_opform::OF_R64:
		if (!operands[0].is_preg()) {
			throw std::runtime_error("expected preg operand 0");
		}

		break;

    // TODO: handle 32-bit regs
	case arm64_opform::OF_R32_R32:
	case arm64_opform::OF_R64_R64:
    // case arm64_opform::OF_R64_R32:
		if (!operands[0].is_preg()) {
			throw std::runtime_error("expected preg operand 0");
		}

		if (!operands[1].is_preg()) {
			throw std::runtime_error("expected preg operand 1");
		}

		break;

	case arm64_opform::OF_R32_M32:
	case arm64_opform::OF_R64_M64: {
		if (!operands[0].is_preg()) {
			throw std::runtime_error("expected preg operand 0");
		}

		if (!operands[1].is_mem()) {
			throw std::runtime_error("expected mem operand 1");
		}

		if (operands[1].memop.virt_base) {
			throw std::runtime_error("expected mem preg base operand 1");
		}

		break;
	}

	// case arm64_opform::OF_M8_R8:
	// case arm64_opform::OF_M16_R16:
	case arm64_opform::OF_M32_R32:
	case arm64_opform::OF_M64_R64: {
		if (!operands[0].is_mem()) {
			throw std::runtime_error("expected mem operand 0");
		}

		if (operands[0].memop.virt_base) {
			throw std::runtime_error("expected mem preg base operand 0");
		}

		if (!operands[1].is_preg()) {
			throw std::runtime_error("expected preg operand 1");
		}

		break;
	}

	// case arm64_opform::OF_M8_I8:
	// case arm64_opform::OF_M16_I16:
	// case arm64_opform::OF_M32_I32:
	// case arm64_opform::OF_M64_I64: {
	// 	if (!operands[0].is_mem()) {
	// 		throw std::runtime_error("expected mem operand 0");
	// 	}

	// 	if (operands[0].memop.virt_base) {
	// 		throw std::runtime_error("expected mem preg base operand 0");
	// 	}

	// 	unsigned long overridden_opcode = raw_opcode;

	// 	// switch (operands[0].memop.seg) {
	// 	// case arm64_register_names::FS:
	// 	// 	overridden_opcode |= FE_SEG(FE_FS);
	// 	// 	break;
	// 	// case arm64_register_names::GS:
	// 	// 	overridden_opcode |= FE_SEG(FE_GS);
	// 	// 	break;
	// 	// default:
	// 	// 	break;
	// 	// }

	// 	if (!operands[1].is_imm()) {
	// 		throw std::runtime_error("expected imm operand 1");
	// 	}
	// 	break;
	// }

	// case arm64_opform::OF_R8_I8:
	// case arm64_opform::OF_R16_I16:
	case arm64_opform::OF_R32_I32:
	case arm64_opform::OF_R64_I64:
		if (!operands[0].is_preg()) {
			throw std::runtime_error("expected preg operand 0");
		}

		if (!operands[1].is_imm()) {
			throw std::runtime_error("expected imm operand 1");
		}
		break;

	case arm64_opform::OF_R64_R64_I64:
		if (!operands[0].is_preg()) {
			throw std::runtime_error("expected preg operand 0");
		}

		if (!operands[1].is_preg()) {
			throw std::runtime_error("expected preg operand 1");
		}

		if (!operands[2].is_imm()) {
			throw std::runtime_error("expected imm operand 2");
		}
		break;

	default:
		throw std::runtime_error("unsupported operand form");
	}

    // TODO: do this for all at once; not one by one
    dump(assembly);
    size = asm_.assemble(assembly.str().c_str(), &encode);

	writer.copy_in(encode, size);

    // TODO: do zero-copy keystone
    ks_free(encode);
}

void arm64_instruction::dump(std::ostream &os) const {
    os << opcode;

	for (size_t i = 0; i < nr_operands; i++) {
		if (operands[i].type != arm64_operand_type::invalid) {
			os << " ";
			operands[i].dump(os);
		}
	}

	os << '\n';
}

// TODO: adapt all
void arm64_operand::dump(std::ostream &os) const {
	switch (type) {
	case arm64_operand_type::imm:
		os << "$0x" << std::hex << immop.u64;
		break;

	case arm64_operand_type::mem:
		os << "[";

		if (memop.virt_base) {
			os << "V" << std::dec << memop.vbase;
		} else {
			os << memop.pbase.to_string();
		}

        if (memop.post_index)
            os << "]";

        os << ", $0x" << std::hex << memop.offset;

        if (memop.pre_index)
            os << "]!";

        // TODO: register indirect with index
		break;

	case arm64_operand_type::preg:
		os << pregop.to_string();
		break;

	case arm64_operand_type::vreg:
		os << "%V" << std::dec << vregop.index;
		break;
    default:
        throw std::runtime_error("arm64_operand::dump() encountered invalid operand type");
	}
}

