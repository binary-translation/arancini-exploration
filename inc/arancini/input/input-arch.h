#pragma once

#include <cstdlib>
#include <memory>
#include <vector>

namespace arancini::ir {
class chunk;
}

namespace arancini::input {
class input_arch {
public:
	virtual ~input_arch() { }
	virtual std::shared_ptr<ir::chunk> translate_chunk(off_t base_address, const void *code, size_t code_size) = 0;
};
} // namespace arancini::input
