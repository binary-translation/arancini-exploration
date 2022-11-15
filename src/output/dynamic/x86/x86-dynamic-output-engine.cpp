#include <arancini/ir/debug-visitor.h>
#include <arancini/ir/node.h>
#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/x86/x86-dynamic-output-engine-impl.h>
#include <arancini/output/dynamic/x86/x86-dynamic-output-engine.h>
#include <iostream>
#include <stdexcept>

using namespace arancini::output::dynamic::x86;
using namespace arancini::ir;

x86_dynamic_output_engine::x86_dynamic_output_engine()
	: oei_(std::make_unique<x86_dynamic_output_engine_impl>())
{
}

x86_dynamic_output_engine::~x86_dynamic_output_engine() = default;

void x86_dynamic_output_engine::lower_prologue(machine_code_writer &writer) { oei_->lower_prologue(writer); }
void x86_dynamic_output_engine::lower_epilogue(machine_code_writer &writer) { oei_->lower_epilogue(writer); }
void x86_dynamic_output_engine::lower(ir::node *node, machine_code_writer &writer) { oei_->lower(node, writer); }

void x86_dynamic_output_engine_impl::lower_prologue(machine_code_writer &writer) { writer.emit8(0xcc); }

void x86_dynamic_output_engine_impl::lower_epilogue(machine_code_writer &writer) { }

void x86_dynamic_output_engine_impl::lower(ir::node *node, machine_code_writer &writer)
{
	debug_visitor v(std::cerr);
	node->accept(v);

	throw std::runtime_error("not implemented");
}
