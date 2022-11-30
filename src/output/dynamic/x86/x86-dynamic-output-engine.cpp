#include <arancini/output/dynamic/x86/x86-dynamic-output-engine.h>
#include <arancini/output/dynamic/x86/x86-translation-context.h>

extern "C" {
#include <xed/xed-interface.h>
}

using namespace arancini::output::dynamic;
using namespace arancini::output::dynamic::x86;

static void initialise_xed()
{
	static bool has_initialised_xed = false;

	if (!has_initialised_xed) {
		xed_tables_init();
		has_initialised_xed = true;
	}
}

x86_dynamic_output_engine::x86_dynamic_output_engine() { initialise_xed(); }

std::shared_ptr<translation_context> x86_dynamic_output_engine::create_translation_context(machine_code_writer &writer)
{
	return std::make_shared<x86_translation_context>(writer);
}
