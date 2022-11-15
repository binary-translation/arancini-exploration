#include <arancini/output/dynamic/arm64/arm64-dynamic-output-engine-impl.h>
#include <arancini/output/dynamic/arm64/arm64-dynamic-output-engine.h>
#include <stdexcept>

using namespace arancini::output::dynamic::arm64;

arm64_dynamic_output_engine::arm64_dynamic_output_engine()
	: oei_(std::make_unique<arm64_dynamic_output_engine_impl>())
{
}

arm64_dynamic_output_engine::~arm64_dynamic_output_engine() = default;

void arm64_dynamic_output_engine::lower(ir::node *node, machine_code_writer &writer) { oei_->lower(node, writer); }

void arm64_dynamic_output_engine_impl::lower(ir::node *node, machine_code_writer &writer) { throw std::runtime_error("not implemented"); }
