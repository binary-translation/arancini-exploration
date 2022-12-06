#pragma once

#include <arancini/output/dynamic/riscv64/machineCode/assembler_riscv.h>
#include <arancini/output/dynamic/translation-context.h>

namespace arancini::output::dynamic::riscv64 {
class riscv64_translation_context : public translation_context {
public:
	riscv64_translation_context(machine_code_writer &writer)
		: translation_context(writer)
	{
	}

	virtual void begin_block() override;
	virtual void begin_instruction(off_t address, const std::string& disasm) override;
	virtual void end_instruction() override;
	virtual void end_block() override;
	virtual void lower(ir::node *n) override;

private:
	Assembler assembler{RV_GC};

	Register materialise(const ir::node *n);
	Register materialiseConstant(int64_t imm);
};
} // namespace arancini::output::dynamic::riscv64
