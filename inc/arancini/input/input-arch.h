#pragma once

#include <cstdlib>
#include <memory>
#include <vector>

namespace arancini::ir {
class ir_builder;
}

namespace arancini::input {
class input_arch {
public:
	input_arch(bool debug = false)
		: debug_(debug)
	{
	}

	virtual ~input_arch() { }
	virtual void translate_chunk(ir::ir_builder &builder, off_t base_address, const void *code, size_t code_size, bool basic_block) = 0;

	bool debug() const { return debug_; }

private:
	bool debug_;
};
} // namespace arancini::input
