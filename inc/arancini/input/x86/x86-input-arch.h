#pragma once

#include <arancini/input/input-arch.h>

namespace arancini::input::x86 {
enum class disassembly_syntax { att, intel };

class x86_input_arch : public input_arch {
public:
	x86_input_arch(bool debug, disassembly_syntax da)
		: input_arch(debug)
		, da_(da)
	{
	}

	virtual ~x86_input_arch() { }
	virtual void translate_chunk(ir::ir_builder &builder, off_t base_address, const void *code, size_t code_size, bool basic_block) override;

private:
	disassembly_syntax da_;
};
} // namespace arancini::input::x86
