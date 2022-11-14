#include <arancini/ir/chunk.h>
#include <arancini/output/dynamic/x86/x86-dynamic-output-engine-impl.h>
#include <arancini/output/dynamic/x86/x86-dynamic-output-engine.h>
#include <arancini/output/mc/machine-code-builder.h>
#include <iostream>

using namespace arancini::output::dynamic::x86;
using namespace arancini::output::mc;

x86_dynamic_output_engine::x86_dynamic_output_engine()
	: oei_(std::make_unique<x86_dynamic_output_engine_impl>())
{
}

x86_dynamic_output_engine::~x86_dynamic_output_engine() = default;

void x86_dynamic_output_engine::generate() { oei_->generate(); }

void x86_dynamic_output_engine_impl::generate() { throw std::runtime_error("not implemented"); }
