#include <arancini/output/dynamic/x86/x86-dynamic-output-engine.h>
#include <arancini/output/dynamic/x86/x86-translation-context.h>

using namespace arancini::output::dynamic;
using namespace arancini::output::dynamic::x86;

std::shared_ptr<translation_context> x86_dynamic_output_engine::create_translation_context(machine_code_writer &writer)
{
	return std::make_shared<x86_translation_context>(writer);
}
