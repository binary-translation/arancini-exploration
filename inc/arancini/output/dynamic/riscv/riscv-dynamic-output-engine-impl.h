#pragma once

namespace arancini::ir {
class node;
}

namespace arancini::output::dynamic {
class machine_code_writer;

namespace riscv {
	class riscv_dynamic_output_engine_impl {
	public:
		void lower(ir::node *node, machine_code_writer &writer);
	};
} // namespace riscv
} // namespace arancini::output::dynamic
