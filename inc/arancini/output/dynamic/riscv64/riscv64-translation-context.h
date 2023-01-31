#pragma once

#include <arancini/ir/node.h>
#include <arancini/output/dynamic/riscv64/encoder/riscv64-assembler.h>
#include <arancini/output/dynamic/translation-context.h>

#include <memory>
#include <variant>

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
	Assembler assembler_{RV_GC};

    std::variant<Register, std::unique_ptr<Label>, std::monostate>
    materialise(const ir::node *n);

	Register materialise_read_reg(const ir::read_reg_node& n);
	void materialise_write_reg(const ir::write_reg_node& n);
	Register materialise_read_mem(const ir::read_mem_node& n);
	void materialise_write_mem(const ir::write_mem_node& n);
    Register materialise_read_pc(const ir::read_pc_node& n);
    void materialise_write_pc(const ir::write_pc_node& n);
    std::unique_ptr<Label> materialise_label(const ir::label_node& n);
    void materialise_br(const ir::br_node& n);
    void materialise_cond_br(const ir::cond_br_node& n);
	Register materialise_constant(int64_t imm);
	Register materialise_unary_arith(const ir::unary_arith_node &n);
	Register materialise_binary_arith(const ir::binary_arith_node &n);
	Register materialise_ternary_arith(const ir::ternary_arith_node &n);
	Register materialise_bit_shift(const ir::bit_shift_node &n);
	Register materialise_bit_extract(const ir::bit_extract_node &n);
	Register materialise_bit_insert(const ir::bit_insert_node &n);
	Register materialise_cast(const ir::cast_node &n);
	std::variant<Register, std::unique_ptr<Label>, std::monostate> materialise_binary_atomic(const ir::binary_atomic_node &n);
};
} // namespace arancini::output::dynamic::riscv64

