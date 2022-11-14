#include <arancini/output/dynamic/riscv/riscv-output-engine-impl.h>
#include <arancini/output/dynamic/riscv/riscv-output-engine.h>

using namespace arancini::output::dynamic::riscv;

riscv_output_engine::riscv_output_engine()
	: oei_(std::make_unique<riscv_output_engine_impl>(chunks()))
{
}

riscv_output_engine::~riscv_output_engine() = default;

void riscv_output_engine::generate(const output_personality &personality) { oei_->generate(); }

void riscv_output_engine_impl::generate()
{
	//
}
