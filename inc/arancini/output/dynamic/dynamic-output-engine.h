#pragma once

namespace arancini::output::dynamic {
class dynamic_output_engine {
public:
	virtual void generate() = 0;
};
} // namespace arancini::output::dynamic
