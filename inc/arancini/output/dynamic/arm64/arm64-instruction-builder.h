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

class variable;

using register_sequence = std::vector<register_operand>;

class scalar final {
public:
    scalar() = default;

    scalar(const register_operand& reg):
        regset_({reg}),
        type_(reg.type())
    {
        // check_type(type_);
    }

    scalar(const register_sequence& regs, ir::value_type type):
        regset_(regs),
        type_(type)
    {
        [[unlikely]]
        if (type_.is_vector())
            throw backend_exception("cannot construct value from vector");

        if (regset_.empty()) return;

        auto element_type = regset_[0].type();
        for (std::size_t i = 1; i < regset_.size(); ++i) {
            if (regset_[i].type() != element_type)
                throw backend_exception("Cannot construct register sequence from registers of different types");
        }
    }

    scalar(const variable& var);

    [[nodiscard]]
    operator const register_operand&() const {
        [[unlikely]]
        if (regset_.size() > 1)
            throw backend_exception("Cannot convert value of type {} to register",
                                    type_);
        return regset_[0];
    }

    [[nodiscard]]
    operator register_operand&() {
        [[unlikely]]
        if (regset_.size() > 1)
            throw backend_exception("Cannot convert value of type {} to register",
                                    type_);
        return regset_[0];
    }

    [[nodiscard]]
    operator register_sequence&() {
        return regset_;
    }

    [[nodiscard]]
    operator const register_sequence&() const {
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

class vector final {
public:
    vector() = default;

    vector(const register_operand& reg):
        regset_({reg}),
        type_(reg.type())
    {
        [[unlikely]]
        if (!type_.is_vector())
            throw backend_exception("cannot construct vector from non-vector");
    }

    vector(const register_sequence& regs, ir::value_type type):
        regset_(regs),
        type_(type)
    {
        [[unlikely]]
        if (!type_.is_vector())
            throw backend_exception("cannot construct vector from non-vector");
    }

    vector(const variable& var);

    [[nodiscard]]
    operator const register_operand&() const {
        [[unlikely]]
        if (regset_.size() > 1)
            throw backend_exception("Cannot convert value of type {} to register",
                                    type_);
        return regset_[0];
    }

    [[nodiscard]]
    operator register_operand&() {
        [[unlikely]]
        if (regset_.size() > 1)
            throw backend_exception("Cannot convert value of type {} to register",
                                    type_);
        return regset_[0];
    }

    [[nodiscard]]
    operator register_sequence&() {
        return regset_;
    }

    [[nodiscard]]
    operator const register_sequence&() const {
        return regset_;
    }

    [[nodiscard]]
    bool vector_backed() const { return regset_.size() == 1; }

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
private:
    register_sequence regset_;
    ir::value_type type_;
};

using value = std::variant<scalar, vector>;

class variable final {
public:
    using scalar_type = class scalar;
    using vector_type = class vector;

    variable() = default;

    variable(const scalar& scalar):
        value_{scalar}
    { }

    variable(const vector& vector):
        value_{vector}
    { }

    operator scalar_type&() {
        return std::get<scalar_type>(value_);
    }

    operator const scalar_type&() const {
        return std::get<scalar_type>(value_);
    }

    operator vector_type&() {
        return std::get<vector_type>(value_);
    }

    operator const vector_type&() const {
        return std::get<vector_type>(value_);
    }

    [[nodiscard]]
    scalar_type& as_scalar() { return std::get<scalar_type>(value_); }

    [[nodiscard]]
    const scalar_type& as_scalar() const { return std::get<scalar_type>(value_); }

    [[nodiscard]]
    vector_type& as_vector() { return std::get<vector_type>(value_); }

    [[nodiscard]]
    const vector_type& as_vector() const { return std::get<vector_type>(value_); }

    ir::value_type type() const {
        if (std::holds_alternative<scalar_type>(value_))
            return std::get<scalar_type>(value_).type();
        return std::get<vector_type>(value_).type();
    }
private:
    std::variant<scalar_type, vector_type> value_;
};

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
    void load(const scalar& destination, const memory_operand& address) {
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
        [[unlikely]]
        if (destination.type().is_vector())
            throw backend_exception("Cannot handle vector loads");

        load(destination.as_scalar(), address);
    }

    void store(const scalar& source, const memory_operand& address) {
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

        store(source.as_scalar(), address);
    }


    void add(const vector& destination, const vector& lhs, const vector& rhs) {
        if (destination.vector_backed() && lhs.vector_backed() && rhs.vector_backed()) {
            append(assembler::add(destination[0], lhs[0], rhs[0]));
        } else {
            for (std::size_t i = 0; i < destination.size(); ++i) {
                append(assembler::add(destination[i], lhs[i], rhs[i]));
            }
        }
    }

	void add(const variable &destination, const variable &lhs, const variable &rhs) {
        if (destination.type().is_vector()) {
            add(destination.as_vector(), lhs.as_vector(), rhs.as_vector());
            return;
        }

        [[unlikely]]
        if (is_bignum(destination.type()))
            return adds(destination.as_scalar(), lhs.as_scalar(), rhs.as_scalar());

        append(assembler::add(destination.as_scalar(), lhs.as_scalar(), rhs.as_scalar()));
    }

    void adds(const scalar &destination, const scalar &lhs, const scalar &rhs) {
        append(assembler::adds(destination[0], lhs[0], rhs[0]));
        for (std::size_t i = 1; i < destination.size(); ++i) {
            append(assembler::adcs(destination[i], lhs[i], rhs[i]));
        }
    }

	void adcs(const scalar& destination, const scalar& top,
              const scalar& lhs, const scalar& rhs)
    {
        // TODO
        for (std::size_t i = 0; i < destination.size(); ++i) {
            comparison(top[i], 0);
            append(assembler::sbcs(register_operand(register_operand::wzr_sp),
                 register_operand(register_operand::wzr_sp),
                 register_operand(register_operand::wzr_sp)));
            append(assembler::adcs(destination[i], lhs[i], rhs[i]));
        }
    }

    void sub(const vector &destination, const vector &lhs, const vector &rhs) {
        if (destination.vector_backed() && lhs.vector_backed() && rhs.vector_backed()) {
            append(assembler::sub(destination, lhs, rhs));
            return;
        }

        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::sub(destination[i], lhs[i], rhs[i]));
        }
    }

    void sub(const variable &destination, const variable &lhs, const variable &rhs) {
        if (destination.type().is_vector()) {
            return sub(destination.as_vector(), lhs.as_vector(), rhs.as_vector());
        }

        [[unlikely]]
        if (is_bignum(destination.type())) {
            return subs(destination.as_scalar(), lhs.as_scalar(), rhs.as_scalar());
        }

        append(assembler::sub(destination.as_scalar(), lhs.as_scalar(), rhs.as_scalar()));
    }

    void subs(const scalar &destination,
              const scalar &lhs,
              const scalar &rhs)
    {
        append(assembler::subs(destination[0], lhs[0], rhs[0]));
        for (std::size_t i = 1; i < destination.size(); ++i) {
            append(assembler::sbcs(destination[i], lhs[i], rhs[i]));
        }
    }

	void sbcs(const scalar& destination, const scalar& top,
              const scalar& lhs, const scalar& rhs)
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

    void logical_or(const scalar& destination, const scalar& lhs, const scalar& rhs) {
        if (is_bignum(destination.type())) {
            for (std::size_t i = 0; i < destination.size(); ++i) {
                append(assembler::orr(destination[i], lhs[i], rhs[i]));
            }
            return;
        }

        const auto& lhs_extended = vreg_alloc_.allocate_scalar(destination.type());
        const auto& rhs_extended = vreg_alloc_.allocate_scalar(destination.type());
        sign_extend(lhs_extended, lhs);
        sign_extend(rhs_extended, rhs);
        append(assembler::orr(destination, lhs_extended, rhs_extended));

        if (destination.type().width() < 64)
            append(assembler::ands(register_operand(register_operand::wzr_sp),
                                   destination, destination));
        else
            append(assembler::ands(register_operand(register_operand::xzr_sp),
                                   destination, destination));

        set_zero_flag();
        set_sign_flag(cond_operand::mi());
    }

    void logical_or(const vector &destination, const vector &lhs, const vector &rhs) {
        if (destination.vector_backed() && lhs.vector_backed() && rhs.vector_backed()) {
            append(assembler::orr(destination, lhs, rhs));
            return;
        }

        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::orr(destination[i], lhs[i], rhs[i]));
        }
    }

    void logical_or(const variable &destination, const variable &lhs, const variable &rhs) {
        if (destination.type().is_vector()) {
            return logical_or(destination.as_vector(), lhs.as_vector(), rhs.as_vector());
        }

        logical_or(destination.as_scalar(), lhs.as_scalar(), rhs.as_scalar());
    }

    void ands(const scalar &destination, const scalar &lhs, const scalar &rhs) {
        [[unlikely]]
        if (is_bignum(destination.type()))
            throw backend_exception("Cannot perform ands {}, {}, {}",
                                    destination.type(), lhs.type(), rhs.type());

        for (std::size_t i = 0; i < destination.size(); ++i) {
            auto lhs_extended = vreg_alloc_.allocate_scalar(destination[i].type());
            auto rhs_extended = vreg_alloc_.allocate_scalar(destination[i].type());
            sign_extend(lhs_extended, lhs[i]);
            sign_extend(rhs_extended, rhs[i]);
            append(assembler::ands(destination[i], lhs_extended, rhs_extended));
        }

        set_zero_flag();
        set_sign_flag(cond_operand::mi());
    }

    void logical_and(const vector &destination, const vector &lhs, const vector &rhs) {
        if (destination.vector_backed() && lhs.vector_backed() && rhs.vector_backed()) {
            append(assembler::and_(destination, lhs, rhs));
            return;
        }

        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::and_(destination[i], lhs[i], rhs[i]));
        }
    }

    void logical_and(const variable &destination,
                     const variable &lhs,
                     const variable &rhs)
    {
        if (destination.type().is_vector()) {
            return logical_and(destination.as_vector(), lhs.as_vector(), rhs.as_vector());
        }

        ands(destination.as_scalar(), lhs.as_scalar(), rhs.as_scalar());
    }

    void exclusive_or(const vector &destination, const vector &lhs, const vector &rhs) {
        if (destination.vector_backed() && lhs.vector_backed() && rhs.vector_backed()) {
            append(assembler::eor(destination, lhs, rhs));
            return;
        }

        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::eor(destination[i], lhs[i], rhs[i]));
        }
    }

    void exclusive_or(const scalar &destination, const scalar &lhs, const scalar &rhs) {
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

    void exclusive_or(const variable &destination, const variable &lhs, const variable &rhs) {
        if (destination.type().is_vector()) {
            exclusive_or(destination.as_vector(), lhs.as_vector(), rhs.as_vector());
            return;
        }

        exclusive_or(destination.as_scalar(), lhs.as_scalar(), rhs.as_scalar());
    }

    void complement(const scalar &destination, const scalar &source) {
        [[unlikely]]
        if (destination.size() != source.size())
            throw backend_exception("Cannot complement {} to {}",
                                    destination.type(), source.type());

        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::mvn(destination, source));
        }
    }

    void complement(const vector &destination, const vector &source) {
        [[unlikely]]
        if (destination.size() != source.size())
            throw backend_exception("Cannot complement {} to {}",
                                    destination.type(), source.type());

        if (destination.vector_backed() && source.vector_backed()) {
            append(assembler::mvn(destination, source));
            return;
        }

        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::mvn(destination[i], source[i]));
        }
    }

    void complement(const variable &destination, const variable &source) {
        if (destination.type().is_vector() && destination.type().is_vector()) {
            complement(destination.as_vector(), source.as_vector());
            return;
        }

        complement(destination.as_scalar(), source.as_scalar());
    }

    void negate(const scalar &destination, const scalar &source) {
        [[unlikely]]
        if (destination.size() != source.size())
            throw backend_exception("Cannot complement {} to {}",
                                    destination.type(), source.type());

        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::neg(destination, source));
        }
    }

    void negate(const vector &destination, const vector &source) {
        [[unlikely]]
        if (destination.size() != source.size())
            throw backend_exception("Cannot complement {} to {}",
                                    destination.type(), source.type());

        if (destination.vector_backed() && source.vector_backed()) {
            append(assembler::neg(destination, source));
        }

        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::neg(destination[i], source[i]));
        }
    }

    void negate(const variable &destination, const variable &source) {
        if (destination.type().is_vector() && source.type().is_vector())
            return negate(destination.as_vector(), source.as_vector());

        return negate(destination.as_scalar(), source.as_scalar());
    }

    void move_to_variable(const scalar& destination, const scalar& source) {
        [[unlikely]]
        if (destination.size() != source.size())
            throw backend_exception("Cannot move type {} to {}", destination.type(), source.type());

        if (destination.type().is_floating_point()) {
            for (std::size_t i = 0; i < destination.size(); ++i) {
                append(assembler::fmov(destination[i], source[i]));
            }
            return;
        }

        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::mov(destination[i], source[i]));
        }
    }

    void move_to_variable(const variable &destination, const variable &source) {
        if (destination.type().is_vector() || source.type().is_vector()) {
            throw backend_exception("Cannot move between vectors");
        }

        move_to_variable(destination.as_scalar(), source.as_scalar());
    }

    void move_to_variable(const scalar& destination, const immediate_operand& imm) {
        [[unlikely]]
        if (destination.type().is_vector())
            throw backend_exception("Cannot move immediate type {} to vector type {}",
                                    imm.type(), destination.type());

        // Convert to unsigned long long so that clzll can be used
        // 1s in sizeof(unsinged long long) - sizeof(imm) upper bits
        auto immediate = util::bit_cast_zeros<unsigned long long>(imm.value());

        // Check the actual size of the scalar
        std::size_t actual_size = std::min(get_min_bitsize(immediate), destination.type().width());

        // Can be moved in one go
        // TODO: implement optimization to support more immediates via shifts
        if (actual_size < 12) {
            insert_comment("Move immediate {:#x} directly as < 12-bits", immediate);
            std::uint64_t mask = (1ULL << actual_size) - 1;
            append(assembler::mov(destination, immediate & mask));
            return;
        }

        // Can be moved in multiple operations
        // NOTE: this assumes that we're only working with 64-bit registers or smaller
        // Determine how many 16-bit chunks we need to move
        std::size_t move_count = actual_size / 16 + (actual_size % 16 != 0);
        insert_comment("Move immediate {:#x} > 12-bits with sequence of movz/movk operations",
                       immediate);
        append(assembler::movz(destination, immediate & 0xFFFF, shift_operand()));
        for (std::size_t i = 1; i < move_count; ++i) {
            auto imm_chunk = immediate >> (i * 16) & 0xFFFF;
            append(assembler::movk(destination, imm_chunk, shift_operand::lsl(i * 16)));
        }
    }

    reg_or_imm move_immediate(const immediate_operand& immediate, ir::value_type reg_type) {
        [[unlikely]]
        if (immediate.type().is_vector() || immediate.type().element_width() > 64)
            throw backend_exception("Attempting to move vector immediate {}", immediate);

        auto imm_val = util::bit_cast_zeros<unsigned long long>(immediate.value());

        std::size_t actual_size = get_min_bitsize(imm_val);
        if (actual_size < immediate.type().element_width()) {
            return immediate_operand(imm_val, immediate.type());
        }

        const auto& destination = vreg_alloc_.allocate_scalar(reg_type);
        move_to_variable(destination, immediate);

        return destination;
     }

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

    scalar cast(const scalar &src, ir::value_type type) {
        throw backend_exception("Not working!");

        insert_comment("Internal cast from {} to {}", src.type(), type);

        if (src.type().type_class() == ir::value_type_class::floating_point &&
            type.type_class() != ir::value_type_class::floating_point) {
            auto dest = vreg_alloc_.allocate_scalar(type);
            append(assembler::fcvtzs(dest, src));
            return dest;
        }

        if (src.type().type_class() == ir::value_type_class::floating_point &&
            type.type_class() == ir::value_type_class::floating_point) {
            if (type.element_width() == 64 && src.type().element_width() == 32) {
                auto dest = vreg_alloc_.allocate_scalar(type);
                append(assembler::fcvt(dest, src));
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

        auto destination = vreg_alloc_.allocate(type).as_scalar();
        sign_extend(destination, src);
        return destination;
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


    void zero_extend(const scalar& destination, const scalar& source) {
        if (destination.type().width() == source.type().width()) {
            move_to_variable(destination, source);
            return;
        }

        // Sanity check
        // TODO: more missing sanity checks
        [[unlikely]]
        if (destination.type().width() < source.type().width())
            throw backend_exception("Cannot zero-extend {} to smaller size {}",
                                    destination.type(), source.type());

        std::size_t current_extension_bytes = 0;

        insert_comment("zero-extend from {} to {}", source.type(), destination.type());
        if (source.type().width() < 8) {
            if (destination.type().width() >= 32) {
                append(assembler::uxtb(destination[0], destination[0]).add_comment("sign-extend"));
            } else {
                backend_exception("Not implemented");
            }
        } else if (source.type().width() == 8) {
            append(assembler::uxtb(destination, source));
        } else if (source.type().width() <= 16) {
            append(assembler::uxth(destination, source));
        } else if (source.type().width() <= 32) {
            append(assembler::uxtw(destination, source));
        }

        current_extension_bytes += destination[0].type().width();
        if (current_extension_bytes >= destination.type().width())
            return;

        for (std::size_t i = 1; i < destination.size(); ++i)
            move_to_variable(destination[i], 0);
    }

    void sign_extend(const scalar& destination, const scalar& source) {
        if (destination.type().width() == source.type().width()) {
            move_to_variable(destination, source);
            return;
        }

        // Sanity check
        // TODO: more missing sanity checks
        [[unlikely]]
        if (destination.type().width() < source.type().width())
            throw backend_exception("Cannot sign-extend {} to smaller size {}",
                                    destination.type(), source.type());

        std::size_t current_extension_bytes = 0;

        // IDEA:
        // 1. Sign-extend reasonably
        // 2. If dest_scalar > 64-bit, determine sign
        // 3. Plaster sign all over the upper bits
        insert_comment("sign-extend from {} to {}", source.type(), destination.type());
        if (source.type().width() < 8) {
            if (destination.type().width() >= 32) {
                append(assembler::lsl(destination[0], source, 7))
                        .add_comment("shift left LSB to set sign bit of byte");
                append(assembler::sxtb(destination[0], destination[0]).add_comment("sign-extend"));
                append(assembler::asr(destination[0], destination[0], 7))
                      .add_comment("shift right to fill LSB with sign bit (except for least-significant bit)");
            } else {
                backend_exception("Not implemented");
            }
        } else if (source.type().width() == 8) {
            append(assembler::sxtb(destination, source));
        } else if (source.type().width() <= 16) {
            append(assembler::sxth(destination, source));
        } else if (source.type().width() <= 32) {
            append(assembler::sxtw(destination, source));
        }

        current_extension_bytes += destination[0].type().width();
        if (current_extension_bytes >= destination.type().width())
            return;

        // Sets the upper bits to 1
        for (std::size_t i = 1; i < destination.size(); ++i)
            append(assembler::asr(destination[i], destination[0], 63));
    }

    void bitcast(const scalar& destination, const scalar& source) {
        // Simply change the meaning of the bit pattern
        // dest_vreg is set to the desired type already, but it must have the
        // scalar of src_vreg
        // A simple mov is sufficient (eliminated anyway by the register
        // allocator)
        insert_comment("Bitcast from {} to {}", source.type(), destination.type());
        // TODO: condition should be changed
        if (destination.size() == 1 && source.size() == 1) {
            move_to_variable(destination, source);
            return;
        }
        throw backend_exception("Cannot handle bitcast");

        if (destination.type().is_vector() || source.type().is_vector()) {
            if (destination.type().element_width() > source.type().element_width()) {
                // Destination consists of fewer elements but of larger widths
                std::size_t dest_idx = 0;
                std::size_t dest_pos = 0;
                for (std::size_t i = 0; i < source.size(); ++i) {
                    left_shift(source[i], source[i], dest_pos % destination.type().element_width());
                    move_to_variable(destination[dest_idx], source);

                    dest_pos += source[i].type().width();
                    dest_idx = (dest_pos / destination[dest_idx].type().width());
                }
            } else if (destination.type().element_width() < source.type().element_width()) {
                // Destination consists of more elements but of smaller widths
                std::size_t src_idx = 0;
                std::size_t src_pos = 0;
                for (std::size_t i = 0; i < destination.size(); ++i) {
                    const auto& src_vreg = vreg_alloc_.allocate(destination[i].type());
                    move_to_variable(src_vreg.as_scalar(), source[src_idx]);
                    append(assembler::lsl(src_vreg.as_scalar(), src_vreg.as_scalar(),
                                          src_pos % source.type().element_width()));
                    move_to_variable(destination[i], src_vreg.as_scalar());

                    src_pos += source[i].type().width();
                    src_idx = (src_pos / source[src_idx].type().width());
                }
            } else {
                for (std::size_t i = 0; i < destination.size(); ++i)
                    move_to_variable(destination[i], source[i]);
            }

            return;
        }

        move_to_variable(destination, source);
    }

    void convert(const scalar& destination, const scalar& source, ir::fp_convert_type trunc_type) {
        throw backend_exception("Not implemented");
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
            move_to_variable(destination, source);
        }
    }

    void truncate(const variable& destination, const variable& source) {

    }

    instruction& branch(const label_operand& target) {
        label_refcount_[target.name()]++;
        return append(assembler::b(target).as_branch());
    }

    // TODO
    instruction& conditional_branch(const label_operand& target, const cond_operand& condition) {
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
    instruction& cbz(const scalar &reg, const label_operand &label) {
        label_refcount_[label.name()]++;
        return append(assembler::cbz(reg, label));
    }

    // TODO: check if this allocated correctly
    // Check reg == 0 and jump if false
    // Otherwise, continue to the next instruction
    // Does not affect condition flags (can be used to compare-and-branch with 1 instruction)
    instruction& cbnz(const scalar &rt, const label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(assembler::cbnz(rt, dest));
    }

    instruction& comparison(const scalar& lhs, const scalar& rhs) {
        if (is_bignum(lhs.type()) || is_bignum(rhs.type()))
            throw backend_exception("Cannot compare {} with {}",
                                    lhs.type(), rhs.type());

        if (lhs.type().is_floating_point()) {
            return append(assembler::fcmp(lhs, rhs));
        }

        return append(assembler::cmp(lhs, rhs));
    }

    instruction& comparison(const scalar& lhs, const immediate_operand& rhs) {
        [[unlikely]]
        if (is_bignum(lhs.type()) || is_bignum(rhs.type()))
            throw backend_exception("Cannot compare {} with immediate {}",
                                    lhs.type(), rhs);

        [[unlikely]]
        if (lhs.type().is_floating_point())
            backend_exception("Cannot compare floating-point register with immediates");

        auto rhs_value = move_immediate(rhs.value(), ir::value_type::u(12));
        return append(assembler::cmp(lhs, rhs_value));
    }

    void left_shift(const scalar& destination, const scalar& input, const scalar& amount) {
        append(assembler::lsl(destination, input, amount));
    }

    void left_shift(const scalar& destination, const scalar& input, const immediate_operand& amount) {
        append(assembler::lsl(destination, input, amount));
    }

    void logical_right_shift(const scalar& destination, const scalar& input, const scalar& amount) {
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

    void arithmetic_right_shift(const scalar& destination, const scalar& input, const scalar& amount) {
        append(assembler::asr(destination, input, amount));
    }

    instruction& conditional_select(const scalar &destination,
                                    const scalar &lhs,
                                    const scalar &rhs,
                                    const cond_operand &cond)
    {
        return append(assembler::csel(destination, lhs, rhs, cond));
    }

    void bit_insert(const scalar& destination, const scalar& source,
                    const scalar& insert_bits, std::size_t to, std::size_t length) {
        [[likely]]
        if (destination.size() == 1) {
            // auto out = builder_.cast(insertion_bits, dest[0].type());
            move_to_variable(destination, source);
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

        bit_insert(destination.as_scalar(), source.as_scalar(), insert_bits.as_scalar(), to, length);
    }

    void bit_extract(const scalar& destination, const scalar& source,
                     std::size_t from, std::size_t length) {
        [[likely]]
        if (destination.size() == 1) {
            // auto out = builder_.cast(insertion_bits, dest[0].type());
            move_to_variable(destination, source);
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

        bit_extract(destination.as_scalar(), source.as_scalar(), from, length);
    }

    void multiply(const scalar& destination,
                  const scalar& multiplicand,
                  const scalar& multiplier)
    {
        [[unlikely]]
        if (!destination.size() || destination.size() > 2 ||
            destination[0].type().is_floating_point())
        {
            throw backend_exception("Invalid multiplication");
        }

        // The input and the output have the same size:
        // For 32-bit multiplication: 64-bit output and signed-extended 32-bit scalars to 64-bit inputs
        // For 64-bit multiplication: 64-bit output and signed-extended 64-bit scalars to 128-bit inputs
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
                const auto& compare_regset = vreg_alloc_.allocate_scalar(destination[0].type());
                move_to_variable(compare_regset, 0xFFFF0000);
                comparison(compare_regset, destination);
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

    void divide(const scalar& destination,
                const scalar& dividend,
                const scalar& divider)
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
        const auto& status = vreg_alloc_.allocate(ir::value_type::u32());
        auto loop_label = fmt::format("loop_{}", instructions_.size());
        auto success_label = fmt::format("success_{}", instructions_.size());
        label(loop_label);
        atomic_load(data, mem);

        body();

        atomic_store(status.as_scalar(), data, mem).add_comment("store if not failure");
        cbz(status.as_scalar(), success_label).add_comment("== 0 represents success storing");
        b(loop_label).add_comment("loop until failure or success");
        label(success_label);
        return;
    }

    void atomic_add(const scalar& destination, const scalar& source,
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

    void atomic_sub(const scalar &destination, const scalar &source,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        const auto& negated = vreg_alloc_.allocate(source.type());
        negate(negated.as_scalar(), source);
        atomic_add(destination, negated.as_scalar(), mem, type);
    }

    void atomic_xadd(const scalar &destination, const scalar &source,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        const scalar &old = vreg_alloc_.allocate_scalar(destination.type());
        append(assembler::mov(old, destination));
        atomic_add(destination, source, mem, type);
        append(assembler::mov(source, old));
    }

    void atomic_clr(const scalar &destination, const scalar &source,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            atomic_block(destination, mem, [this, &destination, &source]() {
                const scalar& negated = vreg_alloc_.allocate_scalar(source.type());
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

    void atomic_and(const scalar &destination, const scalar &source,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        const auto& complemented = vreg_alloc_.allocate_scalar(source.type());
        complement(complemented, source);
        atomic_clr(destination, complemented, mem, type);
    }

    void atomic_eor(const scalar &destination, const scalar &source,
                    const scalar &mem, atomic_types type = atomic_types::exclusive)
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

    void atomic_or(const scalar &destination, const scalar &source,
                    const scalar &mem, atomic_types type = atomic_types::exclusive)
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

    void atomic_smax(const scalar &destination, const scalar &source,
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

    void atomic_smin(const scalar &rm, const scalar &source,
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

    void atomic_umax(const scalar &rm, const scalar &source,
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


    void atomic_umin(const scalar &rm, const scalar &source,
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

    void atomic_swap(const scalar &destination, const scalar &source,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            atomic_block(destination, mem, [this, &destination, &source]() {
                const auto& old = vreg_alloc_.allocate_scalar(destination.type());
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

    void atomic_cmpxchg(const scalar& current, const scalar &acc,
                        const scalar &src, const memory_operand &mem,
                        atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            atomic_block(current, mem, [this, &current, &acc]() {
                comparison(current, acc);
                append(assembler::csel(acc, current, acc, cond_operand::ne()))
                      .add_comment("conditionally move current memory scalar into accumulator");
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

    [[nodiscard]]
    inline std::size_t get_min_bitsize(unsigned long long imm) {
        return value_types::base_type.element_width() - __builtin_clzll(imm|1);
    }
};

inline flag_map_type instruction_builder::flag_map_ = {
    { reg_offsets::ZF, {} },
    { reg_offsets::CF, {} },
    { reg_offsets::OF, {} },
    { reg_offsets::SF, {} },
};

} // namespace arancini::output::dynamic::arm64

