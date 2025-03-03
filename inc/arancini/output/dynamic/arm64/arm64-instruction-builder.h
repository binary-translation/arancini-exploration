#pragma once

#include <arancini/output/dynamic/arm64/arm64-common.h>
#include <arancini/output/dynamic/arm64/variable.h>
#include <arancini/output/dynamic/arm64/arm64-assembler.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <arancini/ir/port.h>
#include <arancini/ir/node.h>
#include <arancini/input/registers.h>
#include <arancini/output/dynamic/machine-code-writer.h>

#include <keystone/keystone.h>

#include <vector>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace arancini::output::dynamic::arm64 {

class virtual_register_allocator final {
public:
    [[nodiscard]]
    variable allocate(ir::value_type type);

    [[nodiscard]]
    scalar allocate_scalar(ir::value_type type);

    [[nodiscard]]
    vector allocate_vector(ir::value_type type);

    void reset() { next_vreg_ = 33; }
private:
    std::size_t next_vreg_ = 33; // TODO: formalize this

    // TODO: shouldn't be using pointers here
    bool base_representable(const ir::value_type& type) {
        return type.width() <= 64; // TODO: formalize this
    }
};

using reg_offsets = arancini::input::x86::reg_offsets;
using flag_map_type = std::unordered_map<reg_offsets, register_operand>;

inline bool is_bignum(ir::value_type type) {
    return type.element_width() > 64;
}

class instruction_builder final {
public:
    void load(const scalar& destination, const memory_operand& address);

    void load(const variable& destination, const memory_operand& address);

    void store(const scalar& source, const memory_operand& address); 

    void store(const variable& source, const memory_operand& address);

	void add(const variable &destination, const variable &lhs, const variable &rhs);

    void adds(const scalar &destination, const scalar &lhs, const scalar &rhs);

	void adcs(const scalar& destination, const scalar& top,
              const scalar& lhs, const scalar& rhs);

    void sub(const vector &destination, const vector &lhs, const vector &rhs);

    void sub(const variable &destination, const variable &lhs, const variable &rhs);

    void subs(const scalar &destination, const scalar &lhs, const scalar &rhs);

	void sbcs(const scalar& destination, const scalar& top,
              const scalar& lhs, const scalar& rhs);

    void logical_or(const scalar& destination, const scalar& lhs, const scalar& rhs);

    void logical_or(const vector &destination, const vector &lhs, const vector &rhs);

    void logical_or(const variable &destination, const variable &lhs, const variable &rhs);

    void ands(const scalar &destination, const scalar &lhs, const scalar &rhs);

    void logical_and(const vector &destination, const vector &lhs, const vector &rhs);

    void logical_and(const variable &destination, const variable &lhs, const variable &rhs);

    void exclusive_or(const vector &destination, const vector &lhs, const vector &rhs);

    void exclusive_or(const scalar &destination, const scalar &lhs, const scalar &rhs);

    void exclusive_or(const variable &destination, const variable &lhs, const variable &rhs);

    void complement(const scalar &destination, const scalar &source);

    void complement(const vector &destination, const vector &source);

    void complement(const variable &destination, const variable &source);

    void negate(const scalar &destination, const scalar &source);

    void negate(const vector &destination, const vector &source);

    void negate(const variable &destination, const variable &source);

    void move_to_variable(const scalar& destination, const scalar& source);

    void move_to_variable(const variable &destination, const variable &source);

    void move_to_variable(const scalar& destination, const immediate_operand& imm);

    scalar copy(const scalar& source);

    vector copy(const vector& source);

    variable copy(const variable& source);

    reg_or_imm move_immediate(const immediate_operand& immediate, ir::value_type reg_type);

    template <typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    reg_or_imm move_immediate(T imm, ir::value_type imm_type) {
        ir::value_type reg_type;
        if (imm_type.element_width() < 32)
            reg_type = ir::value_type(imm_type.type_class(), 32);
        else if (imm_type.element_width() > 32)
            reg_type = ir::value_type(imm_type.type_class(), 64);

        immediate_operand immediate(imm, imm_type);
        return move_immediate(immediate, reg_type);
    }

    scalar cast(const scalar &src, ir::value_type type);

    shift_operand extend_register(const register_operand& reg, arancini::ir::value_type type);

    void zero_extend(const scalar& destination, const scalar& source);

    scalar zero_extend(const scalar& source, ir::value_type type);

    void sign_extend(const scalar& destination, const scalar& source);

    scalar sign_extend(const scalar& source, ir::value_type type);

    void bitcast(const scalar& destination, const scalar& source);

    void bitcast(const variable& destination, const variable& source);

    void convert(const scalar& destination, const scalar& source, ir::fp_convert_type trunc_type);

    void truncate(const scalar& destination, const scalar& source);

    scalar truncate(const scalar& source, ir::value_type type);

    instruction& branch(const label_operand& target);

    instruction& conditional_branch(const label_operand& target, const cond_operand& condition);

    instruction& zero_compare_and_branch(const register_operand& source,
                                         const label_operand& target,
                                         const cond_operand& condition);

    instruction& comparison(const scalar& lhs, const scalar& rhs);

    instruction& comparison(const scalar& lhs, const immediate_operand& rhs);

    void left_shift(const scalar& destination, const scalar& input, const scalar& amount);

    void left_shift(const scalar& destination, const scalar& input, const immediate_operand& shift_amount);

    void logical_right_shift(const scalar& destination, const scalar& input, const scalar& amount);

    void logical_right_shift(const scalar& destination, const scalar& input, const immediate_operand& amount);

    void arithmetic_right_shift(const scalar& destination, const scalar& input, const scalar& amount);

    void arithmetic_right_shift(const scalar& destination, const scalar& input, const immediate_operand& amount);

    void conditional_select(const scalar &destination, const scalar &lhs,
                            const scalar &rhs, const cond_operand &condition)
    {
        [[unlikely]]
        if (destination.size() != lhs.size() || lhs.size() != rhs.size())
            throw backend_exception("Cannot conditionally select between types {} = {} ? {} : {}",
                                    destination.type(), condition, lhs.type(), rhs.type());

        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::csel(destination[i], lhs[i], rhs[i], condition));
        }
    }

    void conditional_set(const scalar &destination, const cond_operand &condition) {
        append(assembler::cset(destination[0], condition));
    }

    void bit_insert(const scalar& destination, const scalar& source,
                    const scalar& insert_bits, std::size_t to, std::size_t length);

    void bit_insert(const variable& destination, const variable& source,
                    const variable& insert_bits, std::size_t to, std::size_t length);

    void bit_extract(const scalar& destination, const scalar& source,
                     std::size_t from, std::size_t length);

    void bit_extract(const variable& destination, const variable& source,
                     std::size_t from, std::size_t length) {
        if (destination.type().is_vector())
            throw backend_exception("Cannot extract from {} to {}",
                                    source.type(), destination.type());

        bit_extract(destination.as_scalar(), source.as_scalar(), from, length);
    }

    void multiply(const scalar& destination, const scalar& multiplicand, const scalar& multiplier);

    void divide(const scalar& destination, const scalar& dividend, const scalar& divider);

    void ret(std::int64_t return_value) {
        append(assembler::mov(dbt_retval_register_, return_value));
        append(assembler::ret());
    }

    instruction& insert_breakpoint(const immediate_operand &index) {
        return append(assembler::brk(index));
    }

    void label(const label_operand &label) {
        if (!labels_.count(label.name())) {
            labels_.insert(label.name());
            append(instruction(label));
            return;
        }
        logger.debug("Label {} already inserted\nCurrent labels:\n{}",
                     label, fmt::format("{}", fmt::join(labels_.begin(), labels_.end(), "\n")));
    }

    enum class atomic_types : std::uint8_t {
        exclusive,
        acquire,
        release,
    };

    instruction& atomic_load(const scalar& destination, const memory_operand& mem,
                             atomic_types type = atomic_types::exclusive); 

    instruction& atomic_store(const register_operand& status, const register_operand& rt,
                              const memory_operand& mem, atomic_types type = atomic_types::exclusive); 

    void atomic_block(const register_operand &data, const memory_operand &mem,
                      std::function<void()> body, atomic_types type = atomic_types::exclusive);

    void atomic_add(const scalar& destination, const scalar& source,
                    const memory_operand& mem, atomic_types type = atomic_types::exclusive);

    void atomic_sub(const scalar &destination, const scalar &source,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive);

    void atomic_xadd(const scalar &destination, const scalar &source,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive);

    void atomic_clr(const scalar &destination, const scalar &source,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive);

    void atomic_and(const scalar &destination, const scalar &source,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive);

    void atomic_eor(const scalar &destination, const scalar &source,
                    const scalar &mem, atomic_types type = atomic_types::exclusive);

    void atomic_or(const scalar &destination, const scalar &source,
                   const scalar &mem, atomic_types type = atomic_types::exclusive);

    void atomic_smax(const scalar &destination, const scalar &source,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive);

    void atomic_smin(const scalar &rm, const scalar &source,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive);

    void atomic_umax(const scalar &rm, const scalar &source,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive);


    void atomic_umin(const scalar &rm, const scalar &source,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive);

    void atomic_swap(const scalar &destination, const scalar &source,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive);

    void atomic_cmpxchg(const scalar& current, const scalar &acc,
                        const scalar &src, const memory_operand &mem,
                        atomic_types type = atomic_types::exclusive);

    [[nodiscard]]
    const register_operand& zero_flag() const { return flag_map_[reg_offsets::ZF]; }

    [[nodiscard]]
    const register_operand& sign_flag() const { return flag_map_[reg_offsets::SF]; }

    [[nodiscard]]
    const register_operand& overflow_flag() const { return flag_map_[reg_offsets::OF]; }

    [[nodiscard]]
    const register_operand& carry_flag() const { return flag_map_[reg_offsets::CF]; }

    [[nodiscard]]
    const register_operand& flag(reg_offsets flag_offset) const { return flag_map_[flag_offset]; }

	void set_zero_flag(const register_operand &destination = flag_map_[reg_offsets::ZF]) {
        append(assembler::cset(destination, cond_operand::eq())).add_comment("compute flag: ZF");
    }

	void set_sign_flag(const cond_operand& cond = cond_operand::lt(),
                       const register_operand &destination = flag_map_[reg_offsets::SF])
    {
        append(assembler::cset(destination, cond)).add_comment("compute flag: SF");
    }

	void set_carry_flag(const cond_operand& cond = cond_operand::cs(),
                        const register_operand &destination = flag_map_[reg_offsets::CF])
    {
        append(assembler::cset(destination, cond)).add_comment("compute flag: CF");
    }

	void set_overflow_flag(const cond_operand& cond = cond_operand::vs(),
                           const register_operand &destination = flag_map_[reg_offsets::OF])
    {
        append(assembler::cset(destination, cond)).add_comment("compute flag: OV");
    }

    void allocate_flags() {
        flag_map_[reg_offsets::ZF] = vreg_alloc_.allocate(ir::value_type::u1()).as_scalar();
        flag_map_[reg_offsets::SF] = vreg_alloc_.allocate(ir::value_type::u1()).as_scalar();
        flag_map_[reg_offsets::OF] = vreg_alloc_.allocate(ir::value_type::u1()).as_scalar();
        flag_map_[reg_offsets::CF] = vreg_alloc_.allocate(ir::value_type::u1()).as_scalar();
    }

    void set_flags(bool inverse_cf) {
        set_zero_flag();
        set_sign_flag();
        set_overflow_flag();
        if (inverse_cf)
            set_carry_flag(cond_operand::cc());
        else
            set_carry_flag();
    }

    void set_and_allocate_flags(bool inverse_cf) {
        allocate_flags();
        set_flags(inverse_cf);
    }

    template <typename... Args>
    void insert_comment(std::string_view format, Args&&... args) {
        append(instruction(fmt::format("// {}", fmt::format(format, std::forward<Args>(args)...))));
    }

	void allocate();

	void emit(machine_code_writer &writer);

	void dump(std::ostream &os) const;

    std::size_t size() const { return instructions_.size(); }

    using instruction_stream = std::vector<instruction>;

    using instruction_stream_iterator = instruction_stream::iterator;
    using const_instruction_stream_iterator = instruction_stream::const_iterator;

    [[nodiscard]]
    instruction_stream_iterator instruction_begin() { return instructions_.begin(); }

    [[nodiscard]]
    const_instruction_stream_iterator instruction_begin() const { return instructions_.begin(); }

    [[nodiscard]]
    const_instruction_stream_iterator instruction_cbegin() const { return instructions_.cbegin(); }

    [[nodiscard]]
    instruction_stream_iterator instruction_end() { return instructions_.end(); }

    [[nodiscard]]
    const_instruction_stream_iterator instruction_end() const { return instructions_.end(); }

    [[nodiscard]]
    const_instruction_stream_iterator instruction_cend() const { return instructions_.cend(); }

    virtual_register_allocator& register_allocator() { return vreg_alloc_; }

    const virtual_register_allocator& register_allocator() const { return vreg_alloc_; }


	instruction& append(const instruction &i) {
        instructions_.push_back(i);
        return instructions_.back();
    }

    const register_operand& context_block() const {
        return context_block_;
    }

    const register_operand& return_value() const {
        return dbt_retval_register_;
    }

    void clear() {
        instructions_.clear();
        vreg_alloc_.reset();
        label_refcount_.clear();
        labels_.clear();
    }
private:
    assembler asm_;
	std::vector<instruction> instructions_;
    virtual_register_allocator vreg_alloc_;
    std::unordered_map<std::string, std::size_t> label_refcount_;
    std::unordered_set<std::string> labels_;

    register_operand context_block_{register_operand::x29};
    register_operand dbt_retval_register_{register_operand::x0};

    static flag_map_type flag_map_;

    void spill();

    [[nodiscard]]
    inline std::size_t get_min_bitsize(unsigned long long imm) {
        return value_types::base_type.element_width() - __builtin_clzll(imm|1);
    }

    void bound_to_type(const scalar& var, ir::value_type type) {
        if (is_bignum(var.type()) || type.is_vector())
            throw backend_exception("Cannot bound big scalar of type {} to type {}", var.type(), type);

        auto mask_immediate = immediate_operand(1 << (var.type().width() - 1), ir::value_type::u(12));
        auto mask = move_immediate(mask_immediate, var.type());
        append(assembler::and_(var, var, mask));
    }

    void bound_to_type(const variable& var, ir::value_type type) {
        if (var.type().is_vector())
            throw backend_exception("Cannot bound to type vectors");

        bound_to_type(var.as_scalar(), type);
    }
};

inline flag_map_type instruction_builder::flag_map_ = {
    { reg_offsets::ZF, {} },
    { reg_offsets::CF, {} },
    { reg_offsets::OF, {} },
    { reg_offsets::SF, {} },
};

} // namespace arancini::output::dynamic::arm64

