#include <arancini/output/x86/x86-output-engine-impl.h>
#include <arancini/output/x86/x86-output-engine.h>
#include <iostream>

using namespace arancini::output::x86;

x86_output_engine::x86_output_engine()
	: oei_(std::make_unique<x86_output_engine_impl>(chunks()))
{
}

x86_output_engine::~x86_output_engine() = default;

void x86_output_engine::generate(const output_personality &personality) { oei_->generate(); }

void x86_output_engine_impl::generate() { std::cerr << "x86 dbt gen" << std::endl; }
