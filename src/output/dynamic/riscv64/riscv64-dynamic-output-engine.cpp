#include <arancini/output/dynamic/riscv64/riscv64-dynamic-output-engine.h>
#include <arancini/output/dynamic/riscv64/riscv64-translation-context.h>
#include <stdexcept>

using namespace arancini::output::dynamic;
using namespace arancini::output::dynamic::riscv64;

std::shared_ptr<translation_context>
riscv64_dynamic_output_engine::create_translation_context(
    machine_code_writer &writer) {
    return std::make_shared<riscv64_translation_context>(writer);
}
