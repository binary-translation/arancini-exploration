#pragma once

namespace arancini::ir {
class node;
}

namespace arancini::output::dynamic {
class machine_code_writer;

class dynamic_output_engine {
public:
	virtual void lower(ir::node *n, machine_code_writer &writer) = 0;
};
} // namespace arancini::output::dynamic
