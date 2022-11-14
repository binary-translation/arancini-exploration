#include <arancini/output/dynamic/riscv/riscv-dynamic-output-engine-impl.h>
#include <arancini/output/dynamic/riscv/riscv-dynamic-output-engine.h>
#include <stdexcept>

using namespace arancini::output::dynamic::riscv;

riscv_dynamic_output_engine::riscv_dynamic_output_engine()
	: oei_(std::make_unique<riscv_dynamic_output_engine_impl>())
{
}

riscv_dynamic_output_engine::~riscv_dynamic_output_engine() = default;

void riscv_dynamic_output_engine::lower(ir::node *node, machine_code_writer &writer) { oei_->lower(node, writer); }

void riscv_dynamic_output_engine_impl::lower(ir::node *node, machine_code_writer &writer) { throw std::runtime_error("not implemented"); }
