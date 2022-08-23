#pragma once

#include <cstdlib>
#include <vector>

namespace arancini::ir {
class builder;
}

namespace arancini::input {
class input_arch {
public:
	virtual ~input_arch() { }
	virtual void translate_code(ir::builder &builder, off_t base_address, const void *code, size_t code_size) = 0;
};
} // namespace arancini::input
