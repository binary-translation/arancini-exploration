#pragma once

#include <arancini/ir/value-type.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>
#include <arancini/output/dynamic/arm64/arm64-instruction-builder.h>

#include <arancini/ir/node.h>
#include <arancini/ir/port.h>
#include <arancini/output/dynamic/translation-context.h>

#include <unordered_map>

namespace arancini::output::dynamic::arm64 {

class arm64_translation_context : public translation_context {
public:
	arm64_translation_context(machine_code_writer &writer);

	virtual void begin_block() override;
	virtual void begin_instruction(off_t address, const std::string &disasm) override;
	virtual void end_instruction() override;
	virtual void end_block() override;
	virtual void lower(const std::shared_ptr<ir::action_node> &n) override;

    virtual ~arm64_translation_context() { }
private:
	instruction_builder builder_;
    std::vector<ir::node *> nodes_;
	std::set<const ir::node *> materialised_nodes_;
	std::unordered_map<const ir::port *, std::vector<register_operand>> port_to_vreg_;
	std::unordered_map<unsigned long, off_t> instruction_index_to_guest_;
    std::unordered_map<const ir::local_var *, std::vector<register_operand>> locals_;
    int ret_;
	int next_vreg_;
	off_t this_pc_;
    size_t instr_cnt_ = 0;

	register_operand alloc_vreg(ir::value_type type) {
        return register_operand(next_vreg_++, type);
    }

	register_operand &alloc_vreg(const ir::port &p, ir::value_type type) {
		auto v = alloc_vreg(type);
        port_to_vreg_[&p].push_back(v);
		return port_to_vreg_[&p].back();
	}

	register_operand &alloc_vreg(const ir::port &p, register_operand &vreg) {
        port_to_vreg_[&p].push_back(vreg);
		return port_to_vreg_[&p].back();
	}

    register_operand &alloc_vreg(const ir::port &p) { return alloc_vreg(p, p.type()); }

    std::vector<register_operand> &alloc_vregs(const ir::port &p);

	register_operand &vreg_for_port(const ir::port &p, size_t index = 0) { return vregs_for_port(p)[index]; }

    std::vector<register_operand> &vregs_for_port(const ir::port &p) { return port_to_vreg_[&p]; }

    std::vector<register_operand> &materialise_port(ir::port &p);


    memory_operand guestreg_memory_operand(int regoff,
                                           memory_operand::addressing_modes mode = memory_operand::addressing_modes::direct);

    void materialise(const ir::node *n);
    void materialise_read_reg(const ir::read_reg_node &n);
    void materialise_write_reg(const ir::write_reg_node &n);
    void materialise_read_mem(const ir::read_mem_node &n);
    void materialise_write_mem(const ir::write_mem_node &n);
    void materialise_read_pc(const ir::read_pc_node &n);
    void materialise_write_pc(const ir::write_pc_node &n);
    void materialise_label(const ir::label_node &n);
    void materialise_br(const ir::br_node &n);
    void materialise_cond_br(const ir::cond_br_node &n);
    void materialise_cast(const ir::cast_node &n);
    void materialise_constant(const ir::constant_node &n);
    void materialise_csel(const ir::csel_node &n);
    void materialise_bit_shift(const ir::bit_shift_node &n);
    void materialise_bit_extract(const ir::bit_extract_node &n);
    void materialise_bit_insert(const ir::bit_insert_node &n);
    void materialise_vector_insert(const ir::vector_insert_node &n);
    void materialise_vector_extract(const ir::vector_extract_node &n);
    void materialise_unary_arith(const ir::unary_arith_node &n);
    void materialise_binary_arith(const ir::binary_arith_node &n);
    void materialise_ternary_arith(const ir::ternary_arith_node &n);
    void materialise_binary_atomic(const ir::binary_atomic_node &n);
    void materialise_ternary_atomic(const ir::ternary_atomic_node &n);
    void materialise_internal_call(const ir::internal_call_node &n);
    void materialise_read_local(const ir::read_local_node &n);
    void materialise_write_local(const ir::write_local_node &n);

    register_operand add_membase(const register_operand &addr, const ir::value_type &t = ir::value_type::u64());

    template <typename T, std::enable_if_t<std::is_arithmetic<T>::value, int> = 0>
    register_operand mov_immediate(T imm, ir::value_type type);

    register_operand cast(const register_operand &op, ir::value_type type);

    enum class reg_offsets : std::uint16_t;
    util::static_map<reg_offsets, register_operand, 4> flag_map;
};

} // namespace arancini::output::dynamic::arm64

