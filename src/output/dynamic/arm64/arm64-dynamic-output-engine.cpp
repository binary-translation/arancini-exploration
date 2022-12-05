#include <arancini/output/dynamic/arm64/arm64-dynamic-output-engine.h>
#include <arancini/output/dynamic/arm64/arm64-translation-context.h>
#include <stdexcept>

using namespace arancini::output::dynamic;
using namespace arancini::output::dynamic::arm64;

std::shared_ptr<translation_context> arm64_dynamic_output_engine::create_translation_context(machine_code_writer &writer)
{
	return std::make_shared<arm64_translation_context>(writer);
}
