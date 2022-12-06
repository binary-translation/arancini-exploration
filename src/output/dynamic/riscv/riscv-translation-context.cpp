#include <arancini/output/dynamic/riscv/riscv-translation-context.h>

using namespace arancini::output::dynamic::riscv;

void riscv_translation_context::begin_block() { }
void riscv_translation_context::begin_instruction(off_t address, const std::string &disasm) { }
void riscv_translation_context::end_instruction() { }
void riscv_translation_context::end_block() { }
void riscv_translation_context::lower(ir::node *n) { }
