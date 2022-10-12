#pragma once

#include <arancini/output/output-engine.h>

namespace arancini::output {
class dynamic_output_personality;
}

namespace arancini::output::x86 {
class x86_output_engine_impl {
public:
	x86_output_engine_impl(const std::vector<std::shared_ptr<ir::chunk>> &chunks)
		: chunks_(chunks)
	{
	}

	void generate(const dynamic_output_personality &personality);

private:
	const std::vector<std::shared_ptr<ir::chunk>> &chunks_;
};
} // namespace arancini::output::x86
