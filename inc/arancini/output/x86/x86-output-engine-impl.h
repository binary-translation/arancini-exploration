#pragma once

#include <arancini/output/output-engine.h>

namespace arancini::output::x86 {
class x86_output_engine_impl {
public:
	x86_output_engine_impl(const std::vector<std::shared_ptr<ir::chunk>> &chunks)
		: chunks_(chunks)
	{
	}

	void generate();

private:
	const std::vector<std::shared_ptr<ir::chunk>> &chunks_;
};
} // namespace arancini::output::x86
