#include <arancini/ir/chunk.h>
#include <arancini/output/mc/machine-code-builder.h>
#include <arancini/output/output-personality.h>
#include <arancini/output/x86/x86-output-engine-impl.h>
#include <arancini/output/x86/x86-output-engine.h>
#include <iostream>

using namespace arancini::output::x86;
using namespace arancini::output::mc;

x86_output_engine::x86_output_engine()
	: oei_(std::make_unique<x86_output_engine_impl>(chunks()))
{
}

x86_output_engine::~x86_output_engine() = default;

void x86_output_engine::generate(const output_personality &personality)
{
	if (personality.kind() == output_personality_kind::personality_dynamic) {
		oei_->generate(*(const dynamic_output_personality *)&personality);
	}
}

void x86_output_engine_impl::generate(const dynamic_output_personality &p)
{
	std::cerr << "x86 dbt gen" << std::endl;

	machine_code_builder mcb(p.get_allocator());
	mcb.write_u8(0);

	//p.entrypoint(mcb.get_base());
}
