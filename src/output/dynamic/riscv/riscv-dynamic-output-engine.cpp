#include <arancini/output/dynamic/riscv/riscv-dynamic-output-engine-impl.h>
#include <arancini/output/dynamic/riscv/riscv-dynamic-output-engine.h>

using namespace arancini::output::dynamic::riscv;

riscv_dynamic_output_engine::riscv_dynamic_output_engine()
	: oei_(std::make_unique<riscv_dynamic_output_engine_impl>())
{
}

riscv_dynamic_output_engine::~riscv_dynamic_output_engine() = default;

void riscv_dynamic_output_engine::generate() { oei_->generate(); }

void riscv_dynamic_output_engine_impl::generate()
{
	//
}
