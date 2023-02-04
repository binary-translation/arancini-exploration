#include <arancini/output/dynamic/arm64/arm64-translation-context.h>

using namespace arancini::output::dynamic::arm64;

void arm64_translation_context::begin_block() { }
void arm64_translation_context::begin_instruction(off_t address, const std::string &disasm) { }
void arm64_translation_context::end_instruction() { }
void arm64_translation_context::end_block() { }
void arm64_translation_context::lower(const std::shared_ptr<ir::action_node> &n) { }

void arm64_translation_context::materialise(const ir::node* n) {

}

