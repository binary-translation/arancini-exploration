#include <arancini/ir/node.h>
#include <arancini/output/dynamic/riscv64/riscv64-translation-context.h>

using namespace arancini::output::dynamic::riscv64;
using namespace arancini::ir;

void riscv64_translation_context::begin_block() { }
void riscv64_translation_context::begin_instruction(off_t address, const std::string &disasm) { }
void riscv64_translation_context::end_instruction() { }
void riscv64_translation_context::end_block() { }
void riscv64_translation_context::lower(ir::node *n) { }
