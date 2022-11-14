#include <arancini/output/dynamic/arm64/arm64-dynamic-output-engine-impl.h>
#include <arancini/output/dynamic/arm64/arm64-dynamic-output-engine.h>

using namespace arancini::output::dynamic::arm64;

arm64_dynamic_output_engine::arm64_dynamic_output_engine()
	: oei_(std::make_unique<arm64_dynamic_output_engine_impl>())
{
}

arm64_dynamic_output_engine::~arm64_dynamic_output_engine() = default;

void arm64_dynamic_output_engine::generate() { oei_->generate(); }

void arm64_dynamic_output_engine_impl::generate()
{
	//
}
