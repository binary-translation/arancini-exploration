#pragma once

#include <arancini/output/dynamic/translation-context.h>

namespace arancini::output::dynamic::riscv {
class riscv_translation_context : public translation_context {
public:
	riscv_translation_context(machine_code_writer &writer)
		: translation_context(writer)
	{
	}

	virtual void begin_block() override;
	virtual void begin_instruction() override;
	virtual void end_instruction() override;
	virtual void end_block() override;
	virtual void lower(ir::node *n) override;
};
} // namespace arancini::output::dynamic::riscv
