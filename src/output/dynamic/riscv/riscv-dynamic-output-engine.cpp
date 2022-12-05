#include <arancini/output/dynamic/riscv/riscv-dynamic-output-engine.h>
#include <arancini/output/dynamic/riscv/riscv-translation-context.h>
#include <stdexcept>

using namespace arancini::output::dynamic;
using namespace arancini::output::dynamic::riscv;

std::shared_ptr<translation_context> riscv_dynamic_output_engine::create_translation_context(machine_code_writer &writer)
{
	return std::make_shared<riscv_translation_context>(writer);
}
