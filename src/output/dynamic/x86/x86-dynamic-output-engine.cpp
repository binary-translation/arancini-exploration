#include <arancini/output/dynamic/x86/x86-dynamic-output-engine-impl.h>
#include <arancini/output/dynamic/x86/x86-dynamic-output-engine.h>
#include <stdexcept>

using namespace arancini::output::dynamic::x86;

x86_dynamic_output_engine::x86_dynamic_output_engine()
	: oei_(std::make_unique<x86_dynamic_output_engine_impl>())
{
}

x86_dynamic_output_engine::~x86_dynamic_output_engine() = default;

void x86_dynamic_output_engine::lower(ir::node *node, machine_code_writer &writer) { oei_->lower(node, writer); }

void x86_dynamic_output_engine_impl::lower(ir::node *node, machine_code_writer &writer) { throw std::runtime_error("not implemented"); }
