#pragma once

#include "arancini/output/dynamic/arm64/arm64-instruction.h"
#include <keystone/keystone.h>

#include <arancini/ir/port.h>
#include <arancini/ir/node.h>
#include <arancini/input/registers.h>
#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/arm64/arm64-assembler.h>

#include <vector>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace arancini::output::dynamic::arm64 {

using register_sequence = std::vector<register_operand>;

class value final {
public:
    value(const register_operand& reg):
        regset_({reg}),
        type_(reg.type())
    { }

    value(const register_sequence& regs, ir::value_type type):
        regset_(regs),
        type_(type)
    { }

    [[nodiscard]]
    operator register_operand() const {
        [[unlikely]]
        if (regset_.size() > 1)
            throw backend_exception("Cannot convert value of type {} to register",
                                    type_);
        return regset_[0];
    }

    operator register_sequence() const {
        return regset_;
    }

    [[nodiscard]]
    ir::value_type type() const { return type_; }

    [[nodiscard]]
    register_operand& operator[](std::size_t i) { return regset_[i]; }

    [[nodiscard]]
    const register_operand& operator[](std::size_t i) const { return regset_[i]; }

    [[nodiscard]]
    register_operand& front() { return regset_[0]; }

    [[nodiscard]]
    const register_operand& front() const { return regset_[0]; }

    [[nodiscard]]
    register_operand& back() { return regset_[regset_.size()]; }

    [[nodiscard]]
    const register_operand& back() const { return regset_[regset_.size()]; }

    [[nodiscard]]
    std::size_t size() const { return regset_.size(); }
    // TODO: make sure this is always possible
    // void cast(ir::value_type type) { type_ = type; }
private:
    register_sequence regset_;
    ir::value_type type_;
};

class variable final {
public:
    // TODO: do we need this?
    variable() = default;

    variable(const value& reg):
        values_{reg}
    { }

    variable(std::initializer_list<register_operand> regs):
        variable(regs.begin(), regs.end())
    { }

    template <typename It>
    variable(It begin, It end):
        values_(begin, end)
    {
        if (values_.empty()) return;
        auto type = values_[0].type();
        for (std::size_t i = 1; i < values_.size(); ++i) {
            if (values_[i].type() != type)
                throw backend_exception("Cannot construct register sequence from registers of different types");
        }
    }

    operator value() const {
        [[unlikely]]
        if (values_.size() > 1)
            throw backend_exception("Accessing register set of {} registers as single register",
                                    values_.size());
        return values_[0];
    }

    operator std::vector<value>() const {
        return values_;
    }

    [[nodiscard]]
    value& operator[](std::size_t i) { return values_[i]; }

    [[nodiscard]]
    const value& operator[](std::size_t i) const { return values_[i]; }

    [[nodiscard]]
    value& front() { return values_[0]; }

    [[nodiscard]]
    const value& front() const { return values_[0]; }

    [[nodiscard]]
    value& back() { return values_[values_.size()]; }

    [[nodiscard]]
    const value& back() const { return values_[values_.size()]; }

    [[nodiscard]]
    std::size_t size() const { return values_.size(); }

    [[nodiscard]]
    ir::value_type type() const {
        if (values_.size() == 1) return values_[0].type();
        return ir::value_type::vector(values_[0].type(), values_.size());
    }

    void push_back(const register_operand& reg) { values_.push_back(reg); }

    void push_back(register_operand&& reg) { values_.push_back(std::move(reg)); }
private:
    std::vector<value> values_;
};

class virtual_register_allocator final {
public:
    [[nodiscard]]
    value allocate(ir::value_type type);

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
    struct immediates_upgrade_policy final {
        friend instruction_builder;

        reg_or_imm operator()(const reg_or_imm& op) {
            if (std::holds_alternative<register_operand>(op.get()))
                return op;

            const immediate_operand& imm = op;
            return builder_->move_immediate(imm.value(), imm_type_, reg_type_);
        }
    protected:
        immediates_upgrade_policy(instruction_builder* builder, ir::value_type imm_type, ir::value_type reg_type):
            builder_(builder),
            imm_type_(imm_type),
            reg_type_(reg_type)
        { }
    private:
        instruction_builder* builder_;
        ir::value_type imm_type_;
        ir::value_type reg_type_;
    };

	void add(const variable &destination, const variable &lhs, const variable &rhs) {
        // TODO: checks
        // TODO: implement via vector
        if (destination.type().is_vector()) {
            for (std::size_t i = 0; i < destination.size(); ++i) {
                append(assembler::add(destination[i], lhs[i], rhs[i]));
            }
            return;
        }

        [[unlikely]]
        if (is_bignum(destination.type()))
            return adds(destination, lhs, rhs);

        append(assembler::add(destination[0], lhs[0], rhs[0]));
    }

    void adds(const value &destination, const value &lhs, const value &rhs) {
        // TODO: shifts
        append(assembler::adds(destination[0], lhs[0], rhs[0]));
        for (std::size_t i = 1; i < destination.size(); ++i) {
            append(assembler::adcs(destination[i], lhs[i], rhs[i]));
        }
    }

	void adcs(const variable& destination,
              const variable& top,
              const variable& lhs,
              const variable& rhs)
    {
        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::cmp(top[i], 0));
            append(assembler::sbcs(register_operand(register_operand::wzr_sp),
                 register_operand(register_operand::wzr_sp),
                 register_operand(register_operand::wzr_sp)));
            append(assembler::adcs(destination[i], lhs[i], rhs[i]));
        }
    }

    void sub(const variable &destination,
             const variable &lhs,
             const variable &rhs)
    {
        if (destination.type().is_vector()) {
            for (std::size_t i = 0; i < destination.size(); ++i) {
                for (std::size_t i = 0; i < destination.size(); ++i) {
                    append(assembler::sub(destination[i], lhs[i], rhs[i]));
                }
            }
        }

        [[unlikely]]
        if (is_bignum(destination.type())) {
            return subs(destination, lhs, rhs);
        }

        append(assembler::sub(destination[0], lhs[0], rhs[0]));
    }

    void subs(const value &destination,
              const value &lhs,
              const value &rhs)
    {
        append(assembler::subs(destination[0], lhs[0], rhs[0]));
        for (std::size_t i = 1; i < destination.size(); ++i) {
            append(assembler::sbcs(destination[i], lhs[i], rhs[i]));
        }
    }

	void sbcs(const value& destination,
              const value& top,
              const value& lhs,
              const value& rhs)
    {
        if (destination.size() != top.size() || top.size() != lhs.size() || lhs.size() != rhs.size())
            throw backend_exception("Cannot perform subtract with borrow for given types");
        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::cmp(top[i], 0));
            append(assembler::sbcs(register_operand(register_operand::wzr_sp),
                 register_operand(register_operand::wzr_sp),
                 register_operand(register_operand::wzr_sp)));
            append(assembler::sbcs(destination[i], lhs[i], rhs[i]));
        }
    }

    void logical_or(const value& destination,
                    const value& lhs,
                    const value& rhs)
    {
        if (is_bignum(destination.type())) {
            for (std::size_t i = 0; i < destination.size(); ++i) {
                append(assembler::orr(destination[i], lhs[i], rhs[i]));
            }
            return;
        }

        switch (destination.type().element_width()) {
        case 1:
            extend_to_byte(lhs);
            extend_to_byte(rhs);
        case 8:
        case 16:
            extend_register(lhs, destination[0].type());
            extend_register(rhs, destination[0].type());
        case 32:
            append(assembler::orr(destination, lhs, rhs));
            break;
        case 64:
            append(assembler::orr(destination, lhs, rhs));
            break;
        default:
            throw backend_exception("Unsupported ORR operation");
        }

        if (destination[0].type().element_width() < 64)
            append(assembler::ands(register_operand(register_operand::wzr_sp),
                                   destination, destination));
        else
            append(assembler::ands(register_operand(register_operand::xzr_sp),
                                   destination, destination));

        set_zero_flag();
        set_sign_flag(cond_operand::mi());
    }

    void logical_or(const variable &destination,
                    const variable &lhs,
                    const variable &rhs)
    {
        if (destination.type().is_vector()) {
            for (std::size_t i = 0; i < destination.size(); ++i) {
                append(assembler::orr(destination[i], lhs[i], rhs[i]));
            }
            return;
        }

        logical_or(destination[0], lhs[0], rhs[0]);
    }

    void logical_and(const variable &destination,
                     const variable &lhs,
                     const variable &rhs)
    {
        if (destination.type().is_vector()) {
            for (std::size_t i = 0; i < destination.size(); ++i) {
                append(assembler::and_(destination[i], lhs[i], rhs[i]));
            }
            return;
        }

        ands(destination[0], lhs[0], rhs[0]);
    }

    void ands(const value &destination,
              const value &lhs,
              const value &rhs)
    {
        [[unlikely]]
        if (is_bignum(destination.type()))
            throw backend_exception("Cannot perform ands {}, {}, {}",
                                    destination.type(), lhs.type(), rhs.type());

        for (std::size_t i = 0; i < destination.size(); ++i) {
            switch (destination[0].type().element_width()) {
            case 1:
                extend_to_byte(lhs);
                extend_to_byte(rhs);
            case 8:
            case 16:
                extend_register(lhs, destination[0].type());
                extend_register(rhs, destination[0].type());
            case 32:
                append(assembler::ands(destination, lhs, rhs));
                break;
            case 64:
                append(assembler::ands(destination, lhs, rhs));
                break;
            default:
                throw backend_exception("Unsupported ORR operation");
            }
        }

        set_zero_flag();
        set_sign_flag(cond_operand::mi());
    }

    void exclusive_or(const value &destination, const value &lhs, const value &rhs) {
        // if (destination[0].type().is_floating_point()) {
        //     lhs[0].cast(value_type::vector(ir::value_type::f64(), 2));
        //     rhs[0].cast(value_type::vector(ir::value_type::f64(), 2));
        //     auto type = destination[0].type();
        //     destination[0].cast(value_type::vector(value_type::f64(), 2));
        //     builder_.eor_(destination[0], lhs[0], rhs[0]);
        //     destination[0].cast(type);
        //     std::size_t i = 1;
        //     for (; i < destination.size(); ++i) {
        //         lhs[i].cast(value_type::vector(value_type::f64(), 2));
        //         rhs[i].cast(value_type::vector(value_type::f64(), 2));
        //         destination[i].cast(value_type::vector(value_type::f64(), 2));
        //         builder_.eor_(destination[i], lhs[i], rhs[i]);
        //         destination[i].cast(type);
        //     }
        //     return;
        // }
        if (destination.type().is_floating_point())
            throw backend_exception("floating-point exclusive-or not implemented");

        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::eor(destination[i], lhs[i], rhs[i]));
        }

        if (destination.size() == 1) {
            if (destination[0].type().element_width() > 32)
                append(assembler::ands(register_operand(register_operand::xzr_sp), destination[0], destination[0]));
            else
                append(assembler::ands(register_operand(register_operand::wzr_sp), destination[0], destination[0]));
            set_zero_flag();
            set_sign_flag(cond_operand::mi());
        }
    }

    void exclusive_or(const variable &destination,
                      const variable &lhs,
                      const variable &rhs)
    {
        if (destination.size() == 0) return;

        if (destination.type().is_vector()) {
            for (std::size_t i = 0; i < destination.size(); ++i) {
                append(assembler::eor(destination[i], lhs[i], rhs[i]));
            }
            return;
        }

        exclusive_or(value(destination), value(lhs), value(rhs));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& complement(const value &dst, const reg_or_imm &src) {
        // TODO
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 6), dst.type());
        if (dst.type().width() == 1)
            return append(assembler::eor(dst, immediates_policy(src), 1));
        return append(assembler::mvn(dst, immediates_policy(src)));
    }

    instruction& negate(const value &dst, const value &src) {
        return append(assembler::neg(dst, src));
    }

    // TODO: expand this to include all move_to_register logic
    // TODO: move_to_register() expects equal sizes
    // TODO: extend() will extend
    instruction& move_to_register(const value &destination, const immediate_operand &immediate) {
        if (destination.type().is_floating_point()) {
            return append(assembler::fmov(destination, immediate));
        }

        immediates_upgrade_policy upgrade(this, ir::value_type(ir::value_type_class::unsigned_integer, 12),
                                          destination.type());
        auto src_conv = upgrade(immediate);
        return append(assembler::mov(destination, src_conv));
    }

    instruction& move_to_register(const variable &destination, const variable &source) {
        // TODO:
        if (destination.type().is_floating_point()) {
            return append(assembler::fmov(destination[0], source[0]));
        }

        return append(assembler::mov(destination[0], source[0]));
    }

    void zero_extend(const value& destination, const value& source) {
        if (source.type().element_width() <= 8) {
            append(assembler::uxtb(destination, source));
            return;
        }

        if (source.type().element_width() <= 16) {
            append(assembler::uxth(destination, source));
            return;
        }

        if (source.type().element_width() <= 32) {
            append(assembler::uxtw(destination, source));
            return;
        }

        if (source.type().element_width() <= 256) {
            for (std::size_t i = 0; i < source.size(); ++i) {
                move_to_register(destination, source);
            }

            // Set upper registers to zero
            if (destination.size() > 1) {
                insert_comment("Set upper registers to zero");
                for (std::size_t i = source.size(); i < destination.size(); ++i)
                    move_to_register(destination[i], 0);
            }
            return;
        }
    }

    void sign_extend(const value& destination, const value& source) {
        // Sanity check
        if (destination.type().width() <= source.type().width())
            throw backend_exception("Cannot sign-extend {} to smaller size {}",
                                    destination.type(), source.type());
        // IDEA:
        // 1. Sign-extend reasonably
        // 2. If dest_value > 64-bit, determine sign
        // 3. Plaster sign all over the upper bits
        insert_comment("sign-extend from {} to {}", source.type(), destination.type());
        if (source.type().width() < 8) {
            // 1 -> N
            // sign-extend to 1 byte
            // sign-extend the rest
            // TODO: this is likely wrong
            append(assembler::lsl(destination, source, 7))
                    .add_comment("shift left LSB to set sign bit of byte");
            append(assembler::sxtb(destination, destination).add_comment("sign-extend"));
            append(assembler::asr(destination, destination, 7))
                  .add_comment("shift right to fill LSB with sign bit (except for least-significant bit)");

            return;
        }

        if (source.type().width() == 8) {
            append(assembler::sxtb(destination, source));
            return;
        }

        if (source.type().width() == 16) {
            append(assembler::sxth(destination, source));
            return;
        }

        if (source.type().width() == 32) {
            append(assembler::sxtw(destination, source));
            return;
        }

        if (source.type().width() == 32) {
            append(assembler::sxtw(destination, source));
            return;
        }

        move_to_register(destination, source);
    }

    void bitcast(const variable& destination, const variable& source) {
        // Simply change the meaning of the bit pattern
        // dest_vreg is set to the desired type already, but it must have the
        // value of src_vreg
        // A simple mov is sufficient (eliminated anyway by the register
        // allocator)
        insert_comment("Bitcast from {} to {}", source.type(), destination.type());

        if (destination.type().is_vector() || source.type().is_vector()) {
            if (destination.type().element_width() > source.type().element_width()) {
                // Destination consists of fewer elements but of larger widths
                std::size_t dest_idx = 0;
                std::size_t dest_pos = 0;
                for (std::size_t i = 0; i < source.size(); ++i) {
                    left_shift(source[i], source[i], dest_pos % destination.type().element_width());
                    move_to_register(destination[dest_idx], source);

                    dest_pos += source[i].type().width();
                    dest_idx = (dest_pos / destination[dest_idx].type().width());
                }
            } else if (destination.type().element_width() < source.type().element_width()) {
                // Destination consists of more elements but of smaller widths
                std::size_t src_idx = 0;
                std::size_t src_pos = 0;
                for (std::size_t i = 0; i < destination.size(); ++i) {
                    const auto& src_vreg = vreg_alloc_.allocate(destination[i].type());
                    move_to_register(src_vreg, source[src_idx]);
                    append(assembler::lsl(src_vreg, src_vreg, src_pos % source.type().element_width()));
                    move_to_register(destination[i], src_vreg);

                    src_pos += source[i].type().width();
                    src_idx = (src_pos / source[src_idx].type().width());
                }
            } else {
                for (std::size_t i = 0; i < destination.size(); ++i)
                    move_to_register(destination[i], source[i]);
            }

            return;
        }

        move_to_register(destination, source);
    }

    void convert(const value& destination, const value& source, ir::fp_convert_type trunc_type) {
        constexpr auto uint_type = ir::value_type_class::unsigned_integer;

        // convert integer to float
        if (source.type().is_integer() && destination.type().is_floating_point()) {
             if (destination.type().type_class() == uint_type)
                append(assembler::ucvtf(destination, source));
             else
                append(assembler::scvtf(destination, source));
        } else if (source.type().is_floating_point() && destination.type().is_integer()) {
            // Handle float/double -> integer conversions
            switch (trunc_type) {
                case ir::fp_convert_type::trunc:
                // if float/double -> truncate to int
                // NOTE: both float/double handled through the same instructions,
                // only register differ
                if (destination.type().type_class() == uint_type)
                    append(assembler::fcvtzu(destination, source));
                else
                    append(assembler::fcvtzs(destination, source));
                break;
            case ir::fp_convert_type::round:
                // if float/double -> round to closest int
                // NOTE: both float/double handled through the same instructions,
                // only register differ
                if (destination.type().type_class() == uint_type)
                    append(assembler::fcvtau(destination, source));
                else
                    append(assembler::fcvtas(destination, source));
                break;
            case ir::fp_convert_type::none:
            default:
                throw backend_exception("Cannot handle truncation type: {}", util::to_underlying(trunc_type));
            }
        } else {
            // converting between different represenations of integers/floating
            // point numbers
            //
            // Destination virtual register set to the correct type upon creation
            // TODO: need to handle different-sized types?
            move_to_register(destination, source);
        }
    }

    void truncate(const variable& destination, const variable& source) {

    }

    instruction& branch(const label_operand& target) {
        label_refcount_[target.name()]++;
        return append(assembler::b(target).as_branch());
    }

    instruction& b(const label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(assembler::b(dest).as_branch());
    }

    instruction& beq(const label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(assembler::beq(dest).as_branch()
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

    instruction& bl(const label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(assembler::bl(dest));
    }

    instruction& bne(const label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(assembler::bne(dest));
    }

    // Check reg == 0 and jump if true
    // Otherwise, continue to the next instruction
    // Does not affect condition flags (can be used to compare-and-branch with 1 instruction)
    instruction& cbz(const value &reg, const label_operand &label) {
        label_refcount_[label.name()]++;
        return append(assembler::cbz(reg, label));
    }

    // TODO: check if this allocated correctly
    // Check reg == 0 and jump if false
    // Otherwise, continue to the next instruction
    // Does not affect condition flags (can be used to compare-and-branch with 1 instruction)
    instruction& cbnz(const value &rt, const label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(assembler::cbnz(rt, dest));
    }

    // TODO: handle register_sequence
    instruction& cmn(const register_operand &dst,
                     const reg_or_imm &src) {
        return append(assembler::cmn(dst, src));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& cmp(const register_operand &dst, const reg_or_imm &src) {
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        return append(assembler::cmp(dst, immediates_policy(src)));
    }

    instruction& fcmp(const register_operand &dest, const register_operand &src) {
        return append(assembler::fcmp(dest, src));
    }

    instruction& comparison(const value& lhs, const value& rhs) {
        if (lhs.type().is_floating_point()) {
            return append(assembler::fcmp(lhs, rhs));
        }

        return append(assembler::cmp(lhs, rhs));
    }

    instruction& comparison(const value& lhs, const immediate_operand& rhs) {
        [[unlikely]]
        if (lhs.type().is_floating_point())
            backend_exception("Cannot compare floating-point register with immediates");

        immediates_upgrade_policy upgrade(this, ir::value_type(ir::value_type_class::unsigned_integer, 12),
                                          lhs.type());
        return append(assembler::cmp(lhs, upgrade(rhs)));
    }

    instruction& tst(const register_operand &dst, const reg_or_imm &src) {
        if (std::holds_alternative<register_operand>(src.get()))
            return append(assembler::tst(dst, register_operand(src)));
        return append(assembler::tst(dst, immediate_operand(src)));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& lsl(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2) {
        std::size_t size = dst.type().element_width() <= 32 ? 4 : 6;
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), dst.type());
        return append(assembler::lsl(dst, src1, immediates_policy(src2)));
    }

    void left_shift(const value& destination, const value& input, const value& amount) {
        append(assembler::lsl(destination, input, amount));
    }

    void left_shift(const value& destination, const value& input, const immediate_operand& amount) {
        append(assembler::lsl(destination, input, amount));
    }

    void logical_right_shift(const value& destination, const value& input, const value& amount) {
        append(assembler::lsr(destination, input, amount));

        // {
        //     auto amount_imm = reinterpret_cast<const constant_node*>(n.amount().owner())->const_val_i();
        //     if (amount_imm < 64) {
        //         builder_.extr(dest_vreg[0], input[1], input[0], amount_imm);
        //         builder_.lsr(dest_vreg[1], input[1], amount);
        //     } else if (amount_imm == 64) {
        //         builder_.mov(dest_vreg[0], input[1]);
        //         builder_.mov(dest_vreg[1], 0);
        //     } else {
        //         throw backend_exception("Unsupported logical right-shift operation with amount {}", amount_imm);
        //     }
        // }
    }

    void arithmetic_right_shift(const value& destination, const value& input, const value& amount) {
        append(assembler::asr(destination, input, amount));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& lsr(const register_operand &dst,
                     const register_operand &src1,
                     const reg_or_imm &src2) {
        std::size_t size = dst.type().element_width() <= 32 ? 5 : 6;
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), dst.type());
        return append(assembler::lsr(dst, src1, immediates_policy(src2)));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& asr(const register_operand &dst,
                     const register_operand &src1,
                     const reg_or_imm &src2) {
        std::size_t size = dst.type().element_width() <= 32 ? 5 : 6;
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), dst.type());
        return append(assembler::asr(dst, src1, immediates_policy(src2)));
    }

    instruction& extr(const register_operand &dst,
                      const register_operand &src1,
                      const register_operand &src2,
                      const immediate_operand &shift) {
        return append(assembler::extr(dst, src1, src2, shift));
    }

    instruction& conditional_select(const value &destination,
                                    const value &lhs,
                                    const value &rhs,
                                    const cond_operand &cond)
    {
        return append(assembler::csel(destination, lhs, rhs, cond));
    }

    instruction& cset(const register_operand &dst,
                      const cond_operand &cond) {
        return append(assembler::cset(dst, cond));
    }

    instruction& bfxil(const register_operand &dst,
                       const register_operand &src1,
                       const immediate_operand &lsb,
                       const immediate_operand &width)
    {
        return append(assembler::bfxil(dst, src1, lsb, width));
    }

    instruction& ubfx(const register_operand &dst,
                      const register_operand &src1,
                      const immediate_operand &lsb,
                      const immediate_operand &width)
    {
        return append(assembler::ubfx(dst, src1, lsb, width));
    }

    instruction& bfi(const register_operand &dst,
                     const register_operand &src1,
                     const immediate_operand &lsb,
                     const immediate_operand &width)
    {
        return append(assembler::bfi(dst, src1, lsb, width));
    }

    void bit_insert(const value& destination, const value& source,
                    const value& insert_bits, std::size_t to, std::size_t length) {
        [[likely]]
        if (destination.size() == 1) {
            // auto out = builder_.cast(insertion_bits, dest[0].type());
            move_to_register(destination, source);
            append(assembler::bfi(destination, insert_bits, to, length));
            return;
        }
    }

    void bit_insert(const variable& destination, const variable& source,
                    const variable& insert_bits, std::size_t to, std::size_t length) {
        if (destination.type().is_vector()) {
            throw backend_exception("TODO");
            return;
        }

        bit_insert(value(destination), value(source), value(insert_bits), to, length);
    }

    void bit_extract(const value& destination, const value& source,
                     std::size_t from, std::size_t length) {
        [[likely]]
        if (destination.size() == 1) {
            // auto out = builder_.cast(insertion_bits, dest[0].type());
            move_to_register(destination, source);
            append(assembler::ubfx(destination, source, from, length));
            return;
        }
    }

    void bit_extract(const variable& destination, const variable& source,
                     std::size_t from, std::size_t length) {
        if (destination.type().is_vector()) {
            throw backend_exception("TODO");
            return;
        }

        bit_extract(value(destination), value(source), from, length);
    }

    void load(const value& destination, const memory_operand& address) {
        for (std::size_t i = 0; i < destination.size(); ++i) {
            if (destination[i].type().element_width() <= 8) {
                memory_operand memory(address.base_register(), address.offset().value() + i);
                append(assembler::ldrb(destination[i], memory));
            } else if (destination[i].type().element_width() <= 16) {
                memory_operand memory(address.base_register(), address.offset().value() + i * 2);
                append(assembler::ldrh(destination[i], memory));
            } else {
                auto offset = i * (destination[i].type().element_width() < 64 ? 4 : 8);
                memory_operand memory(address.base_register(), address.offset().value() + offset);
                append(assembler::ldr(destination[i], memory));
            }
        }
    }

    void load(const variable& destination, const memory_operand& address) {
        if (destination.type().is_vector())
            throw backend_exception("Cannot handle vector stores");

        load(destination[0], address);
    }

    void store(const value& source, const memory_operand& address) {
        for (std::size_t i = 0; i < source.size(); ++i) {
            if (source[i].type().element_width() <= 8) {
                memory_operand memory(address.base_register(), address.offset().value() + i);
                append(assembler::strb(source[i], memory));
            } else if (source[i].type().element_width() <= 16) {
                memory_operand memory(address.base_register(), address.offset().value() + i * 2);
                append(assembler::strh(source[i], memory));
            } else {
                auto offset = i * (source[i].type().element_width() < 64 ? 4 : 8);
                memory_operand memory(address.base_register(), address.offset().value() + offset);
                append(assembler::str(source[i], memory));
            }
        }
    }

    void store(const variable& source, const memory_operand& address) {
        if (source.type().is_vector())
            throw backend_exception("Cannot handle vector stores");

        store(source[0], address);
    }

    void multiply(const value& destination,
                  const value& multiplicand,
                  const value& multiplier)
    {
        [[unlikely]]
        if (!destination.size() || destination.size() > 2 ||
            destination[0].type().is_floating_point())
        {
            throw backend_exception("Invalid multiplication");
        }

        // The input and the output have the same size:
        // For 32-bit multiplication: 64-bit output and signed-extended 32-bit values to 64-bit inputs
        // For 64-bit multiplication: 64-bit output and signed-extended 64-bit values to 128-bit inputs
        // NOTE: this is very unfortunate
        bool sets_flags = true;
        if (destination.size() == 1) {
            switch (destination[0].type().type_class()) {
            case ir::value_type_class::signed_integer:
                {
                    auto lhs = cast(multiplicand[0], ir::value_type(multiplicand[0].type().type_class(), 32, 1));
                    auto rhs = cast(multiplier[0], ir::value_type(multiplier[0].type().type_class(), 32, 1));
                    append(assembler::smull(destination, lhs, rhs));
                }
                break;
            case ir::value_type_class::unsigned_integer:
                {
                    auto lhs = cast(multiplicand[0], ir::value_type(multiplicand[0].type().type_class(), 32, 1));
                    auto rhs = cast(multiplier[0], ir::value_type(multiplier[0].type().type_class(), 32, 1));
                    append(assembler::umull(destination, lhs, rhs));
                }
                break;
            case ir::value_type_class::floating_point:
                // The same fmul is used in both 32-bit and 64-bit multiplication
                // The actual operation depends on the type of its registers
                {
                    auto multiplicand_conv = cast(multiplicand, destination[0].type());
                    auto multiplier_conv = cast(multiplier, destination[0].type());
                    append(assembler::fmul(destination, multiplicand_conv, multiplier_conv));
                    sets_flags = false;
                }
                break;
            default:
                throw backend_exception("Encounted unknown type class {} for multiplication",
                                        util::to_underlying(destination[0].type().type_class()));
            }
            // TODO: need to compute CF and OF
            // CF and OF are set to 1 when multiplicand * multiplier > 64-bits
            // Otherwise they are set to 0
            if (sets_flags) {
                auto compare_regset = vreg_alloc_.allocate(destination[0].type());
                move_to_register(compare_regset, 0xFFFF0000);
                cmp(compare_regset, destination);
                set_carry_flag(cond_operand::ne());
                set_overflow_flag(cond_operand::ne());
                sets_flags = false;
            }
            return;
        }

        [[likely]]
        // Get lower 64 bits
        append(assembler::mul(destination[0], multiplicand[0], multiplier[0]));

        // Get upper 64 bits
        switch (destination[1].type().type_class()) {
        case ir::value_type_class::signed_integer:
            append(assembler::smulh(destination[1], multiplicand[0], multiplier[0]));
            break;
        case ir::value_type_class::unsigned_integer:
            append(assembler::umulh(destination[1], multiplicand[0], multiplier[0]));
            break;
        default:
            throw backend_exception("Encounted unknown type class {} for multiplication",
                                    util::to_underlying(destination[1].type().type_class()));
        }

        // TODO: need to compute CF and OF
        // CF and OF are set to 1 when multiplicand * multiplier > 64-bits
        // Otherwise they are set to 0
        append(assembler::cmp(destination[1], 0));
        set_carry_flag(cond_operand::ne());
        set_overflow_flag(cond_operand::ne());

        return;
    }

    void divide(const value& destination,
                const value& dividend,
                const value& divider)
    {
        if (destination.size() > 1)
            throw backend_exception("Division not supported for types larger than 128-bits");

        // The input and the output have the same size:
        // For 64-bit division: 64-bit input dividend/divisor and 64-bit output but 32-bit division
        // For 128-bit multiplication: 128-bit input dividend/divisor and 128-bit output but 64-bit division
        // NOTE: this is very unfortunate
        // NOTE: we'll need to handle separetely floats
        switch (destination[0].type().type_class()) {
        case ir::value_type_class::signed_integer:
            append(assembler::sdiv(destination, dividend, divider));
            break;
        case ir::value_type_class::unsigned_integer:
            append(assembler::udiv(destination, dividend, divider));
            break;
        default:
            throw backend_exception("Encounted unknown type class {} for division",
                                    util::to_underlying(destination[0].type().type_class()));
        }

		return;
    }

    instruction& mul(const register_operand &dest,
                     const register_operand &src1,
                     const register_operand &src2) {
        return append(assembler::mul(dest, src1, src2));
    }

    instruction& smulh(const register_operand &dest,
                       const register_operand &src1,
                       const register_operand &src2) {
        return append(assembler::smulh(dest, src1, src2));
    }

    instruction& smull(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(assembler::smull(dest, src1, src2));
    }

    instruction& umulh(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(assembler::umulh(dest, src1, src2));
    }

    instruction& umull(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(assembler::umull(dest, src1, src2));
    }

    instruction& sdiv(const register_operand &dest,
              const register_operand &src1,
              const register_operand &src2) {
        return append(assembler::sdiv(dest, src1, src2));
    }

    instruction& udiv(const register_operand &dest,
              const register_operand &src1,
              const register_operand &src2) {
        return append(assembler::udiv(dest, src1, src2));
    }

    instruction& fmul(const register_operand &dest,
                      const register_operand &src1,
                      const register_operand &src2) {
        return append(assembler::fmul(dest, src1, src2));
    }

    instruction& fmov(const register_operand &dest, const reg_or_imm &src) {
        return append(assembler::fmov(dest, src));
    }

    instruction& fcvt(const register_operand &dest, const register_operand &src) {
        return append(assembler::fcvt(dest, src));
    }

    instruction& fcvtzs(const register_operand &dest, const register_operand &src) {
        return append(assembler::fcvtzs(dest, src));
    }

    instruction& fcvtzu(const register_operand &dest, const register_operand &src) {
        return append(assembler::fcvtzu(dest, src));
    }

    instruction& fcvtas(const register_operand &dest, const register_operand &src) {
        return append(assembler::fcvtas(dest, src));
    }

    instruction& fcvtau(const register_operand &dest, const register_operand &src) {
        return append(assembler::fcvtau(dest, src));
    }

    // TODO: this also has an immediate variant
    instruction& scvtf(const register_operand &dest,
               const register_operand &src) {
        return append(assembler::scvtf(dest, src));
    }

    instruction& ucvtf(const register_operand &dest,
               const register_operand &src) {
        return append(assembler::ucvtf(dest, src));
    }

    instruction& mrs(const register_operand &dest,
             const register_operand &src) {
        return append(assembler::mrs(dest, src));
    }

    instruction& msr(const register_operand &dest,
                     const register_operand &src) {
        return append(assembler::msr(dest, src));
    }

    instruction& ret() {
        return append(assembler::ret());
    }

    instruction& brk(const immediate_operand &imm) {
        return append(assembler::brk(imm));
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

    instruction& cfinv() {
        return append(assembler::cfinv());
    }

    enum class atomic_types : std::uint8_t {
        exclusive,
        acquire,
        release,
    };

    instruction& atomic_load(const register_operand& dst, const memory_operand& mem,
                             atomic_types type = atomic_types::exclusive) {
        if (type == atomic_types::exclusive) {
            switch (dst.type().element_width()) {
            case 8:
                return append(assembler::ldxrb(dst, mem));
            case 16:
                return append(assembler::ldxrh(dst, mem));
            case 32:
            case 64:
                return append(assembler::ldxr(dst, mem));
            default:
                throw backend_exception("Cannot load atomically type {}", dst.type());
            }
        }

        switch (dst.type().element_width()) {
        case 8:
            return append(assembler::ldaxrb(dst, mem));
        case 16:
            return append(assembler::ldaxrh(dst, mem));
        case 32:
        case 64:
            return append(assembler::ldaxr(dst, mem));
        default:
            throw backend_exception("Cannot load atomically type {}", dst.type());
        }
    }

    instruction& atomic_store(const register_operand& status, const register_operand& rt,
                              const memory_operand& mem, atomic_types type = atomic_types::exclusive) {
        if (type == atomic_types::exclusive) {
            switch (status.type().element_width()) {
            case 8:
                return append(assembler::stxrb(status, rt, mem));
            case 16:
                return append(assembler::stxrh(status, rt, mem));
            case 32:
            case 64:
                return append(assembler::stxr(status, rt, mem));
            default:
                throw backend_exception("Cannot store atomically type {}", status.type());
            }
        }

        switch (status.type().element_width()) {
        case 8:
            return append(assembler::stlxrb(status, rt, mem));
        case 16:
            return append(assembler::stlxrh(status, rt, mem));
        case 32:
        case 64:
            return append(assembler::stlxr(status, rt, mem));
        default:
            throw backend_exception("Cannot store atomically type {}", status.type());
        }
    }

    void atomic_block(const register_operand &data, const memory_operand &mem,
                      std::function<void()> body, atomic_types type = atomic_types::exclusive)
    {
        const register_operand& status = vreg_alloc_.allocate(ir::value_type::u32());
        auto loop_label = fmt::format("loop_{}", instructions_.size());
        auto success_label = fmt::format("success_{}", instructions_.size());
        label(loop_label);
        atomic_load(data, mem);

        body();

        atomic_store(status, data, mem).add_comment("store if not failure");
        cbz(status, success_label).add_comment("== 0 represents success storing");
        b(loop_label).add_comment("loop until failure or success");
        label(success_label);
        return;
    }

    void atomic_add(const value& destination, const value& source,
                    const memory_operand& mem, atomic_types type = atomic_types::exclusive)
    {

        if constexpr (!supports_lse) {
            atomic_block(destination, mem, [this, &destination, &source]() {
                adds(destination, source, destination);
            }, type);
        }

        if (type == atomic_types::exclusive) {
            switch (destination.type().element_width()) {
            case 8:
                append(assembler::ldaddb(destination, source, mem).as_keep());
                return;
            case 16:
                append(assembler::ldaddh(destination, source, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldadd(destination, source, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically add type {}", source.type());
            }
        }

        // TODO
    }

    void atomic_sub(const value &destination, const value &source,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        const auto& negated = vreg_alloc_.allocate(source.type());
        negate(negated, source);
        atomic_add(destination, negated, mem, type);
    }

    void atomic_xadd(const value &destination, const value &source,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        const value &old = vreg_alloc_.allocate(destination.type());
        append(assembler::mov(old, destination));
        atomic_add(destination, source, mem, type);
        append(assembler::mov(source, old));
    }

    void atomic_clr(const value &destination, const value &source,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            atomic_block(destination, mem, [this, &destination, &source]() {
                const value& negated = vreg_alloc_.allocate(source.type());
                complement(negated, source);
                ands(destination, destination, negated);
            }, type);
        }

        if (type == atomic_types::exclusive) {
            switch (destination.type().element_width()) {
            case 8:
                append(assembler::ldclrb(destination, source, mem).as_keep());
                return;
            case 16:
                append(assembler::ldclrh(destination, source, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldclr(destination, source, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", source.type());
            }
        }

        // TODO
    }

    void atomic_and(const value &destination, const value &source,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        const auto& complemented = vreg_alloc_.allocate(source.type());
        complement(complemented, source);
        atomic_clr(destination, complemented, mem, type);
    }

    void atomic_eor(const value &destination, const value &source,
                    const value &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            atomic_block(destination, mem, [this, &destination, &source]() {
                exclusive_or(destination, destination, source);
            }, type);
        }
        if (type == atomic_types::exclusive) {
            switch (destination.type().element_width()) {
            case 8:
                append(assembler::ldeorb(destination, source, mem).as_keep());
                return;
            case 16:
                append(assembler::ldeorh(destination, source, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldeor(destination, source, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", source.type());
            }
        }

        // TODO
    }

    void atomic_or(const value &destination, const value &source,
                    const value &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            atomic_block(destination, mem, [this, &destination, &source]() {
                logical_or(destination, destination, source);
            }, type);
        }
        if (type == atomic_types::exclusive) {
            switch (destination.type().element_width()) {
            case 8:
                append(assembler::ldsetb(destination, source, mem));
                return;
            case 16:
                append(assembler::ldseth(destination, source, mem));
                return;
            case 32:
            case 64:
                append(assembler::ldset(destination, source, mem));
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", destination.type());
            }
        }

        // TODO
    }

    void atomic_smax(const value &destination, const value &source,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            throw backend_exception("smax not implemented");
        }
        if (type == atomic_types::exclusive) {
            switch (destination.type().element_width()) {
            case 8:
                append(assembler::ldsmaxb(destination, source, mem).as_keep());
                return;
            case 16:
                append(assembler::ldsmaxh(destination, source, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldsmax(destination, source, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", source.type());
            }
        }

        // TODO
    }

    void atomic_smin(const value &rm, const value &source,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            throw backend_exception("smin not implemented");
        }
        if (type == atomic_types::exclusive) {
            switch (rm.type().element_width()) {
            case 8:
                append(assembler::ldsminb(rm, source, mem).as_keep());
                return;
            case 16:
                append(assembler::ldsminh(rm, source, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldsmin(rm, source, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", source.type());
            }
        }

        // TODO
    }

    void atomic_umax(const value &rm, const value &source,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            throw backend_exception("umax not implemented");
        }

        if (type == atomic_types::exclusive) {
            switch (rm.type().element_width()) {
            case 8:
                append(assembler::ldumaxb(rm, source, mem).as_keep());
                return;
            case 16:
                append(assembler::ldumaxh(rm, source, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldumax(rm, source, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", source.type());
            }
        }

        // TODO
    }


    void atomic_umin(const value &rm, const value &source,
                            const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            throw backend_exception("umin not implemented");
        }
        if (type == atomic_types::exclusive) {
            switch (rm.type().element_width()) {
            case 8:
                append(assembler::lduminb(rm, source, mem).as_keep());
                return;
            case 16:
                append(assembler::lduminh(rm, source, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldumin(rm, source, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", source.type());
            }
        }

        // TODO
    }

    void atomic_swap(const value &destination, const value &source,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            atomic_block(destination, mem, [this, &destination, &source]() {
                const auto& old = vreg_alloc_.allocate(destination.type());
                append(assembler::mov(old, destination));
                append(assembler::mov(destination, source));
                append(assembler::mov(source, old));
            }, type);
        }

        if (type == atomic_types::exclusive) {
            switch (destination.type().element_width()) {
            case 8:
                append(assembler::swpb(destination, source, mem).as_keep());
                return;
            case 16:
                append(assembler::swph(destination, source, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::swp(destination, source, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", source.type());
            }
        }

        // TODO
    }

    void atomic_cmpxchg(const value& current, const value &acc,
                        const value &src, const memory_operand &mem,
                        atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            atomic_block(current, mem, [this, &current, &acc]() {
                cmp(current, acc).add_comment("compare with accumulator");
                append(assembler::csel(acc, current, acc, cond_operand::ne()))
                      .add_comment("conditionally move current memory value into accumulator");
            }, type);
            return;
        }

        insert_comment("Atomic CMPXCHG using CAS (enabled on systems with LSE support");
        append(assembler::cas(acc, src, mem))
               .add_comment("write source to memory if source == accumulator, accumulator = source");
        append(assembler::cmp(acc, 0));
        append(assembler::mov(current, acc));

        // TODO
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

    [[nodiscard]]
    inline std::size_t get_min_bitsize(unsigned long long imm) {
        return value_types::base_type.element_width() - __builtin_clzll(imm|1);
    }

    template <typename T, std::enable_if_t<std::is_arithmetic<T>::value, int> = 0>
    reg_or_imm move_immediate(T imm, ir::value_type imm_type, ir::value_type reg_type) {
        [[unlikely]]
        if (imm_type.is_vector() || imm_type.element_width() > value_types::base_type.element_width())
            throw backend_exception("Attempting to move immediate {:#x} into unsupported immediate type {}",
                                     imm, imm_type);

        auto immediate = util::bit_cast_zeros<unsigned long long>(imm);
        std::size_t actual_size = get_min_bitsize(immediate);

        if (actual_size < imm_type.element_width()) {
            logger.debug("Immediate {:#x} fits within {} (actual size = {})\n",
                          imm, imm_type, actual_size);
            return immediate_operand(imm, imm_type);
        }

        return move_to_register(imm, reg_type);
     }

    template <typename T, std::enable_if_t<std::is_arithmetic<T>::value, int> = 0>
    reg_or_imm move_immediate(T imm, ir::value_type imm_type) {
        ir::value_type reg_type;
        if (imm_type.element_width() < 32)
            reg_type = ir::value_type(imm_type.type_class(), 32);
        else if (imm_type.element_width() > 32)
            reg_type = ir::value_type(imm_type.type_class(), 64);

        return move_immediate(imm, imm_type, reg_type);
    }

    template <typename T, std::enable_if_t<std::is_arithmetic<T>::value, int> = 0>
    register_operand move_to_register(T imm, ir::value_type type) {
        // Sanity checks
        static_assert (sizeof(T) <= sizeof(std::uint64_t),
                       "Attempting to move immediate requiring more than 64-bits into register");
        static_assert (sizeof(std::uint64_t) <= sizeof(unsigned long long),
                       "ARM DBT expects unsigned long long to be at least as large as 64-bits");

        [[unlikely]]
        if (type.is_vector())
            throw backend_exception("Cannot move immediate {} into vector type {}", imm, type);

        // Convert to unsigned long long so that clzll can be used
        // 1s in sizeof(unsinged long long) - sizeof(imm) upper bits
        auto immediate = util::bit_cast_zeros<unsigned long long>(imm);

        // Check the actual size of the value
        std::size_t actual_size = get_min_bitsize(immediate);
        if (actual_size > type.width()) {
            logger.warn("Converting value of size {} to size {} by truncation\n",
                        actual_size, type.element_width());
            actual_size = type.element_width();
        }

        // Can be moved in one go
        // TODO: implement optimization to support more immediates via shifts
        if (actual_size < 12) {
            insert_comment("Move immediate {:#x} directly as < 12-bits", immediate);
            auto reg = vreg_alloc_.allocate(type);
            std::uint64_t mask = (1ULL << actual_size) - 1;
            // mov<immediates_strict_policy>(reg, immediate & mask);
            // TODO: fix
            move_to_register(reg, immediate & mask);
            return reg;
        }

        // Determine how many 16-bit chunks we need to move
        std::size_t move_count = actual_size / 16 + (actual_size % 16 != 0);
        logger.debug("Moving value {:#x} requires {} 16-bit moves\n", immediate, move_count);

        // Can be moved in multiple operations
        // NOTE: this assumes that we're only working with 64-bit registers or smaller
        auto reg = vreg_alloc_.allocate(type);
        insert_comment("Move immediate {:#x} > 12-bits with sequence of movz/movk operations", immediate);
        append(assembler::movz(reg, immediate & 0xFFFF, shift_operand()));
        for (std::size_t i = 1; i < move_count; ++i) {
            auto imm_chunk = immediate >> (i * 16) & 0xFFFF;
            append(assembler::movk(reg, imm_chunk, shift_operand(shift_operand::shift_type::lsl, i * 16)));
        }

        return reg;
    }

    // TODO: this is wrong
    void extend_to_byte(const register_operand& reg, std::uint8_t idx = 1) {
        lsl(reg, reg, 8 - idx).add_comment("shift left LSB to set sign bit of byte");
        asr(reg, reg, 8 - idx).add_comment("shift right to fill LSB with sign bit (except for least-significant bit)");
    }

    value cast(const value &src, ir::value_type type) {
        insert_comment("Internal cast from {} to {}", src.type(), type);

        if (src.type().type_class() == ir::value_type_class::floating_point &&
            type.type_class() != ir::value_type_class::floating_point) {
            auto dest = vreg_alloc_.allocate(type);
            fcvtzs(dest, src);
            return dest;
        }

        if (src.type().type_class() == ir::value_type_class::floating_point &&
            type.type_class() == ir::value_type_class::floating_point) {
            if (type.element_width() == 64 && src.type().element_width() == 32) {
                auto dest = vreg_alloc_.allocate(type);
                fcvt(dest, src);
                return dest;
            }

            if (type.element_width() == 64 && src.type().element_width() == 64)
                return src;

            throw backend_exception("Cannot internally cast from {} to {}", src.type(), type);
        }

        if (src.type().element_width() >= type.element_width()) {
            return register_operand(src[0].index(), type);
        }

        if (type.element_width() > 64)
            type = ir::value_type::u64();

        auto dest = vreg_alloc_.allocate(type);
        switch (src.type().element_width()) {
        case 1:
            extend_to_byte(src, 1);
        case 8:
            append(assembler::sxtb(dest, src));
            break;
        case 16:
            append(assembler::sxth(dest, src));
            break;
        case 32:
            append(assembler::sxtw(dest, src));
            break;
        default:
            return src;
        }
        return dest;
    }

    shift_operand extend_register(const register_operand& reg, arancini::ir::value_type type) {
        auto mod = shift_operand::shift_type::lsl;

        switch (type.element_width()) {
        case 8:
            if (type.type_class() == ir::value_type_class::signed_integer) {
                mod = shift_operand::shift_type::sxtb;
                append(assembler::sxtb(reg, reg));
            } else {
                mod = shift_operand::shift_type::uxtb;
                append(assembler::uxtb(reg, reg));
            }
            break;
        case 16:
            if (type.type_class() == ir::value_type_class::signed_integer) {
                mod = shift_operand::shift_type::sxth;
                append(assembler::sxth(reg, reg));
            } else {
                mod = shift_operand::shift_type::uxth;
                append(assembler::uxth(reg, reg));
            }
            break;
        }

        return shift_operand(mod, 0);
    }

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
        append(assembler::cset(destination, cond));
    }

	void set_overflow_flag(const cond_operand& cond = cond_operand::vs(),
                           const register_operand &destination = flag_map_[reg_offsets::OF])
    {
        append(assembler::cset(destination, cond));
    }

    void allocate_flags() {
        flag_map_[reg_offsets::ZF] = vreg_alloc_.allocate(ir::value_type::u1());
        flag_map_[reg_offsets::SF] = vreg_alloc_.allocate(ir::value_type::u1());
        flag_map_[reg_offsets::OF] = vreg_alloc_.allocate(ir::value_type::u1());
        flag_map_[reg_offsets::CF] = vreg_alloc_.allocate(ir::value_type::u1());
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

	instruction& append(const instruction &i) {
        instructions_.push_back(i);
        return instructions_.back();
    }

    void clear() {
        instructions_.clear();
        vreg_alloc_.reset();
        label_refcount_.clear();
        labels_.clear();
    }
private:
    assembler asm_;
    constexpr static bool supports_lse = false;
	std::vector<instruction> instructions_;
    virtual_register_allocator vreg_alloc_;
    std::unordered_map<std::string, std::size_t> label_refcount_;
    std::unordered_set<std::string> labels_;

    static flag_map_type flag_map_;

    void spill();
};

inline flag_map_type instruction_builder::flag_map_ = {
    { reg_offsets::ZF, {} },
    { reg_offsets::CF, {} },
    { reg_offsets::OF, {} },
    { reg_offsets::SF, {} },
};

} // namespace arancini::output::dynamic::arm64

