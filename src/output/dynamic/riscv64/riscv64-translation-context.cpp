#include <arancini/ir/node.h>
#include <arancini/output/dynamic/riscv64/riscv64-translation-context.h>

using namespace arancini::output::dynamic::riscv64;
using namespace arancini::ir;

void riscv64_translation_context::begin_block() { }
void riscv64_translation_context::begin_instruction(off_t address, const std::string &disasm) { }
void riscv64_translation_context::end_instruction() { }
void riscv64_translation_context::end_block() { }
void riscv64_translation_context::lower(ir::node *n) { materialise(n); }
Register riscv64_translation_context::materialise(const node *n)
{
	switch (n->kind()) {
	case node_kinds::write_reg: {

		auto n2 = (write_reg_node *)n;
		port &value = n2->value();
		Register regVal = materialise(value.owner());
		assembler.sd(regVal, { FP, static_cast<intptr_t>(n2->regoff()) });

	} break;
	default:
		throw std::runtime_error("unsupported node");
	}

}
