#include <arancini/output/arm64/arm64-output-engine-impl.h>
#include <arancini/output/arm64/arm64-output-engine.h>

using namespace arancini::output::arm64;

arm64_output_engine::arm64_output_engine()
	: oei_(std::make_unique<arm64_output_engine_impl>(chunks()))
{
}

arm64_output_engine::~arm64_output_engine() = default;

void arm64_output_engine::generate(const output_personality &personality) { oei_->generate(); }

void arm64_output_engine_impl::generate()
{
	//
}
