#pragma once

#include <arancini/input/input-arch.h>
#include <arancini/input/x86/x86-internal-functions.h>

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

	virtual ir::internal_function_resolver &get_internal_function_resolver() override { return ifr_; }

private:
	disassembly_syntax da_;
	x86_internal_functions ifr_;
};
} // namespace arancini::input::x86
