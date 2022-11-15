#pragma once

namespace arancini::ir {
class node;
}

namespace arancini::output::dynamic {
class machine_code_writer;

namespace x86 {
	class x86_dynamic_output_engine_impl {
	public:
		void lower(ir::node *node, machine_code_writer &writer);
	};
} // namespace x86
} // namespace arancini::output::dynamic
