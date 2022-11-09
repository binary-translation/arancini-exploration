#pragma once

#include <arancini/input/input-arch.h>

namespace arancini::input::x86 {
class x86_input_arch : public input_arch {
public:
	virtual ~x86_input_arch() { }
	virtual void translate_chunk(ir::ir_builder &builder, off_t base_address, const void *code, size_t code_size, bool basic_block) override;
};
} // namespace arancini::input::x86
