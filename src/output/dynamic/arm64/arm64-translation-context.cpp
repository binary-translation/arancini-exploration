#include "arancini/ir/node.h"
#include "arancini/output/dynamic/arm64/arm64-instruction.h"
#include <arancini/output/dynamic/arm64/arm64-translation-context.h>

#include <arancini/runtime/exec/x86/x86-cpu-state.h>

#include <cmath>
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>

using namespace arancini::output::dynamic::arm64;
using namespace arancini::ir;

void arm64_translation_context::begin_block() { }
void arm64_translation_context::begin_instruction(off_t address, const std::string &disasm) { }
void arm64_translation_context::end_instruction() { }
void arm64_translation_context::end_block() { }
void arm64_translation_context::lower(const std::shared_ptr<ir::action_node> &n) { }
