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
        check_type(regset_, type_);
    }

    scalar(const register_sequence& regs, ir::value_type type):
        regset_(regs),
        type_(type)
    {
        check_type(regset_, type_);
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
    const ir::value_type& type() const { return type_; }

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

    void cast(ir::value_type type) {
        if (regset_.size() == 1) {
            type_ = type;
            regset_[0].cast(type_);
            return;
        }
        throw backend_exception("Attempting to cast large scalar");
    }
private:
    register_sequence regset_;
    ir::value_type type_;

    static void check_type(const register_sequence& regseq, ir::value_type type) {
        [[unlikely]]
        if (type.is_vector())
            throw backend_exception("cannot construct scalar from vector");

        if (regseq.empty()) return;

        auto element_type = regseq[0].type();
        for (std::size_t i = 1; i < regseq.size(); ++i) {
            if (regseq[i].type() != element_type)
                throw backend_exception("Cannot construct register sequence from registers of different types");
        }

        [[unlikely]]
        if (element_type.is_vector())
            throw backend_exception("cannot construct scalar from register {}", regseq[0].type());
    }
};

using scalar_sequence = std::vector<scalar>;

class vector final {
public:
    vector() = default;

    vector(const register_operand& reg):
        backing_vector_(reg),
        type_(reg.type())
    {
        [[unlikely]]
        if (!type_.is_vector())
            throw backend_exception("cannot construct vector from non-vector type {}", type_);
    }

    vector(std::initializer_list<scalar> scalars, ir::value_type type):
        vector(scalars.begin(), scalars.end(), type)
    { }

    template <typename It>
    vector(It start, It end, ir::value_type type):
        type_(type)
    {
        [[unlikely]]
        if (!type_.is_vector())
            throw backend_exception("cannot construct vector from non-vector type {}", type_);

        if (std::distance(start, end) == 1) {
            *this = vector(*start);
            return;
        }

        backing_vector_ = scalar_sequence(start, end);
        for (const auto& scalar : as_simulated_vector()) {
            [[unlikely]]
            if (scalar.type() != type_.element_type())
                throw backend_exception("cannot construct vector of type {} from elements of type {}",
                                        type_, scalar.type());
        }
    }

    [[nodiscard]]
    operator const register_operand&() const {
        return as_backing_vector();
    }

    [[nodiscard]]
    operator register_operand&() {
        return as_backing_vector();
    }

    [[nodiscard]]
    operator scalar_sequence&() {
        return as_simulated_vector();
    }

    [[nodiscard]]
    operator const scalar_sequence&() const {
        return as_simulated_vector();
    }

    [[nodiscard]]
    bool vector_backed() const {
        return std::holds_alternative<register_operand>(backing_vector_);
    }

    [[nodiscard]]
    register_operand& as_backing_vector() {
        [[unlikely]]
        if (!vector_backed())
            throw backend_exception("Attempting to access simulated vector as vector-backed vector");

        return std::get<register_operand>(backing_vector_);
    }

    [[nodiscard]]
    const register_operand& as_backing_vector() const {
        [[unlikely]]
        if (!vector_backed())
            throw backend_exception("Attempting to access simulated vector as vector-backed vector");

        return std::get<register_operand>(backing_vector_);
    }

    [[nodiscard]]
    scalar_sequence& as_simulated_vector() {
        [[unlikely]]
        if (vector_backed())
            throw backend_exception("Attempting to access vector-backed vector as simulated vector");

        return std::get<scalar_sequence>(backing_vector_);
    }

    [[nodiscard]]
    const scalar_sequence& as_simulated_vector() const {
        [[unlikely]]
        if (vector_backed())
            throw backend_exception("Attempting to access vector-backed vector as simulated vector");

        return std::get<scalar_sequence>(backing_vector_);
    }

    [[nodiscard]]
    scalar& operator[](std::size_t i) {
        return as_simulated_vector()[i];
    }

    [[nodiscard]]
    const scalar& operator[](std::size_t i) const {
        return as_simulated_vector()[i];
    }

    [[nodiscard]]
    std::size_t size() const {
        if (vector_backed()) return 1;
        return as_simulated_vector().size();
    }

    [[nodiscard]]
    ir::value_type type() const { return type_; }
private:
    std::variant<std::vector<scalar>, register_operand> backing_vector_;
    ir::value_type type_;

    static void check_type(const register_sequence& regseq, ir::value_type type) {
        [[unlikely]]
        if (!type.is_vector())
            throw backend_exception("cannot construct vector from scalar");

        if (regseq.empty()) return;

        auto element_type = regseq[0].type();
        for (std::size_t i = 1; i < regseq.size(); ++i) {
            [[unlikely]]
            if (regseq[i].type() != element_type)
                throw backend_exception("Cannot construct register sequence from registers of different types");
        }

        [[unlikely]]
        if (!element_type.is_vector())
            throw backend_exception("Cannot construct vector from register {}", regseq[0]);
    }
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
        if (std::holds_alternative<scalar_type>(value_))
            return std::get<scalar_type>(value_);
        throw backend_exception("Accessing vector type {} as scalar",
                                std::get<vector_type>(value_).type());
    }

    operator const scalar_type&() const {
        if (std::holds_alternative<scalar_type>(value_))
            return std::get<scalar_type>(value_);
        throw backend_exception("Accessing vector type {} as scalar",
                                std::get<vector_type>(value_).type());
    }

    operator vector_type&() {
        if (std::holds_alternative<vector_type>(value_))
            return std::get<vector_type>(value_);
        throw backend_exception("Accessing scalar type {} as vector",
                                std::get<scalar_type>(value_).type());
    }

    operator const vector_type&() const {
        if (std::holds_alternative<vector_type>(value_))
            return std::get<vector_type>(value_);
        throw backend_exception("Accessing scalar type {} as vector",
                                std::get<scalar_type>(value_).type());
    }

    [[nodiscard]]
    scalar_type& as_scalar() {
        if (std::holds_alternative<scalar_type>(value_))
            return std::get<scalar_type>(value_);
        throw backend_exception("Accessing vector type {} as scalar",
                                std::get<vector_type>(value_).type());
    }

    [[nodiscard]]
    const scalar_type& as_scalar() const {
        if (std::holds_alternative<scalar_type>(value_))
            return std::get<scalar_type>(value_);
        throw backend_exception("Accessing vector type {} as scalar",
                                std::get<vector_type>(value_).type());
    }

    [[nodiscard]]
    vector_type& as_vector() {
        if (std::holds_alternative<vector_type>(value_))
            return std::get<vector_type>(value_);
        throw backend_exception("Accessing scalar type {} as vector",
                                std::get<scalar_type>(value_).type());
    }

    [[nodiscard]]
    const vector_type& as_vector() const {
        if (std::holds_alternative<vector_type>(value_))
            return std::get<vector_type>(value_);
        throw backend_exception("Accessing scalar type {} as vector",
                                std::get<scalar_type>(value_).type());
    }

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
        if (source.type().is_vector()) {
            auto src = source.as_vector();

            [[unlikely]]
            if (src.vector_backed())
                throw backend_exception("Cannot handle vector stores for type {}", source.type());

            auto simulated = src.as_simulated_vector();
            for (std::size_t i = 0; i < simulated.size(); ++i) {
                auto addr = memory_operand(address.base_register(),
                                           i * (simulated[i].type().width() / 64));
                store(simulated[i], addr);
            }

            return;
        }

        store(source.as_scalar(), address);
    }

	void add(const variable &destination, const variable &lhs, const variable &rhs) {
        if (destination.type().is_vector()) {
            const auto& dest = destination.as_vector();
            const auto& lhs_vec = lhs.as_vector();
            const auto& rhs_vec = rhs.as_vector();
            if (dest.vector_backed() && lhs_vec.vector_backed() && rhs_vec.vector_backed()) {
                append(assembler::add(dest, lhs_vec, rhs_vec));
            } else {
                for (std::size_t i = 0; i < dest.size(); ++i) {
                    append(assembler::add(dest[i], lhs_vec[i], rhs_vec[i]));
                }
            }

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

        auto lhs_extended = sign_extend(lhs, destination.type());
        auto rhs_extended = sign_extend(rhs, destination.type());
        append(assembler::orr(destination, lhs_extended, rhs_extended));
        bound_to_type(destination, destination.type());

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
            auto lhs_extended = sign_extend(lhs[i], destination.type());
            auto rhs_extended = sign_extend(rhs[i], destination.type());
            append(assembler::ands(destination[i], lhs_extended, rhs_extended));
            bound_to_type(destination, destination.type());
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

        insert_comment("Computing complement of {} to {}", destination.type(), source.type());
        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::mvn(destination[i], source[i]));
            bound_to_type(destination[i], destination[i].type());
        }
    }

    void complement(const vector &destination, const vector &source) {
        [[unlikely]]
        if (destination.size() != source.size())
            throw backend_exception("Cannot complement {} to {}",
                                    destination.type(), source.type());

        insert_comment("Computing complement of {} to {}", destination.type(), source.type());
        if (destination.vector_backed() && source.vector_backed()) {
            append(assembler::mvn(destination, source));
            return;
        }

        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::mvn(destination[i], source[i]));
            bound_to_type(destination[i], destination[i].type());
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
            throw backend_exception("Cannot negate {} to {}",
                                    destination.type(), source.type());

        insert_comment("Computing negate of {} to {}", destination.type(), source.type());
        for (std::size_t i = 0; i < destination.size(); ++i) {
            append(assembler::neg(destination, source));
        }
    }

    void negate(const vector &destination, const vector &source) {
        [[unlikely]]
        if (destination.size() != source.size())
            throw backend_exception("Cannot negate {} to {}",
                                    destination.type(), source.type());

        insert_comment("Computing negate of {} to {}", destination.type(), source.type());
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

        [[unlikely]]
        if (destination.type().element_width() != source.type().element_width())
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
        if (destination.type().is_vector()) {
            if (!source.type().is_vector() || destination.type().element_width() != source.type().element_width())
                throw backend_exception("Cannot move type {} to {}", destination.type(), source.type());

            auto dest = destination.as_vector();
            auto src = source.as_vector();
            if (dest.vector_backed() || src.vector_backed())
                throw backend_exception("Cannot move vector-backed type {} to {}",
                        dest.type(), src.type());

            for (std::size_t i = 0; i < dest.size(); ++i)
                move_to_variable(dest[i], src[i]);

            return;
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

    scalar copy(const scalar& source) {
        auto destination = vreg_alloc_.allocate_scalar(source.type());
        move_to_variable(destination, source);
        return destination;
    }

    vector copy(const vector& source) {
        auto destination = vreg_alloc_.allocate_vector(source.type());
        move_to_variable(destination, source);
        return destination;
    }

    variable copy(const variable& source) {
        auto destination = vreg_alloc_.allocate(source.type());
        move_to_variable(destination, source);
        return destination;
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

        return sign_extend(src, type);
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

        [[unlikely]]
        if (is_bignum(source.type()))
            throw backend_exception("Not implemented zero-extend for type {}", source.type());

        // Sanity check
        // TODO: more missing sanity checks
        [[unlikely]]
        if (destination.type().width() < source.type().width())
            throw backend_exception("Cannot zero-extend {} to smaller size {}",
                                    source.type(), destination.type());

        std::size_t current_extension_bytes = 0;
        insert_comment("zero-extend from {} to {}", source.type(), destination.type());
        if (source.type().width() < 8) {
            append(assembler::uxtb(destination[0], source[0]));
            bound_to_type(destination[0], source.type());
        } else if (source.type().width() == 8) {
            append(assembler::uxtb(destination[0], source));
        } else if (source.type().width() <= 16) {
            append(assembler::uxth(destination[0], source));
        } else if (source.type().width() <= 32) {
            append(assembler::uxtw(destination[0], source));
        } else {
            move_to_variable(destination[0], source[0]);
        }

        current_extension_bytes += destination[0].type().width();
        if (current_extension_bytes >= destination.type().width())
            return;

        for (std::size_t i = 1; i < destination.size(); ++i)
            move_to_variable(destination[i], 0);
    }

    scalar zero_extend(const scalar& source, ir::value_type type) {
        const auto& destination = vreg_alloc_.allocate_scalar(type);
        zero_extend(destination, source);
        return destination;
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
        if (source.type().width() == 1) {
            if (destination.type().width() == 32) {
                append(assembler::lsl(destination[0], source, 7))
                        .add_comment("shift left LSB to set sign bit of byte");
                append(assembler::sxtb(destination[0], destination[0]).add_comment("sign-extend"));
                append(assembler::asr(destination[0], destination[0], 7))
                      .add_comment("shift right to fill LSB with sign bit (except for least-significant bit)");
            } else if (destination.type().width() > 32) {
                backend_exception("Not implemented");
            } else {
                backend_exception("Not implemented");
            }
        } else if (source.type().width() == 8) {
            append(assembler::sxtb(destination[0], source));
        } else if (source.type().width() <= 16) {
            append(assembler::sxth(destination[0], source));
        } else if (source.type().width() <= 32) {
            append(assembler::sxtw(destination[0], source));
        } else if (source.type().width() == 64) {
            move_to_variable(destination[0], source);
        }

        current_extension_bytes += destination[0].type().width();
        if (current_extension_bytes >= destination.type().width())
            return;

        // Sets the upper bits to 1
        for (std::size_t i = 1; i < destination.size(); ++i)
            append(assembler::asr(destination[i], destination[0], 63));
    }

    scalar sign_extend(const scalar& source, ir::value_type type) {
        const auto& destination = vreg_alloc_.allocate_scalar(type);
        sign_extend(destination, source);
        return destination;
    }

    void bitcast(const scalar& destination, const scalar& source) {
        // Simply change the meaning of the bit pattern
        // dest_vreg is set to the desired type already, but it must have the
        // scalar of src_vreg
        // A simple mov is sufficient (eliminated anyway by the register
        // allocator)
        insert_comment("Bitcast from {} to {}", source.type(), destination.type());
        move_to_variable(destination, source);
        return;
    }

    void bitcast(const variable& destination, const variable& source) {
        if (!destination.type().is_vector() && !source.type().is_vector())
            return bitcast(destination.as_scalar(), source.as_scalar());

        [[unlikely]]
        if (destination.type().is_vector() && source.type().is_vector())
            throw backend_exception("Cannot handle bitcast from vector {} to vector {}",
                                    source.type(), destination.type());

        if (destination.type().width() != source.type().width())
            throw backend_exception("Cannot bitcast from type {} to type with different width {}",
                                    source.type(), destination.type());

        if (destination.type().is_vector()) {
            auto dest = destination.as_vector();
            auto src = source.as_scalar();

            [[unlikely]]
            if (dest.vector_backed())
                throw backend_exception("Cannot handle bitcast for vector-based vector {}",
                                        dest.type());

            if (dest.type().element_width() == src.type().element_width()) {
                for (std::size_t i = 0; i < dest.size(); ++i)
                    move_to_variable(dest[i], src[i]);
                return;
            }

            // TODO: check
            // [[unlikely]]
            // if (dest.type().element_width() % src.type().element_width())
            //     throw backend_exception("Cannot move incompatible type {} to {}",
            //                             src.type(), dest.type());

            if (src.type().element_width() <= dest.type().element_width()) {
                for (std::size_t i = 0; i < dest.size(); ++i) {
                    for (std::size_t j = 0; j < dest[i].size(); ++j)
                        move_to_variable(dest[i][j], src[i+j]);
                }
            } else {
                bit_insert(dest, dest, src, 0, src.type().width());
            }

            return;
        }

        if (source.type().is_vector()) {
            auto dest = destination.as_scalar();
            auto src = source.as_vector();

            [[unlikely]]
            if (src.vector_backed())
                throw backend_exception("Cannot handle bitcast from vector-based vector {}",
                                        src.type());

            if (src.type().element_width() <= dest.type().element_width()) {
                std::size_t insert_per_elem = dest[0].type().width() / src[0].type().width();
                std::size_t inserted_count = 0;
                for (std::size_t i = 0; i < dest.size(); ++i) {
                    auto insert_idx = i + inserted_count;
                    bit_insert(dest[i], dest[i], src[insert_idx],
                               inserted_count * src[insert_idx].type().width(),
                               src[insert_idx].type().width());

                    if (++inserted_count == insert_per_elem)
                        inserted_count = 0;
                }

                return;
            }

            throw backend_exception("Not implemented");
        }

        // Source is vector
        auto src = source.as_vector();
        auto dest = destination.as_scalar();

        [[unlikely]]
        if (src.vector_backed())
            throw backend_exception("Cannot handle bitcast for vector-based vector {}",
                                    src.type());

        for (std::size_t i = 0; i < dest.size(); ++i)
            move_to_variable(dest[i], src[i]);
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

    void truncate(const scalar& destination, const scalar& source) {
        [[unlikely]]
        if (destination.type().width() > source.type().width())
            throw backend_exception("Cannot truncate from {} to larger type {}",
                                    destination.type(), source.type());

        [[unlikely]]
        if (is_bignum(destination.type()))
            throw backend_exception("Truncation not implemented for large scalar type {}",
                                    destination.type());

        insert_comment("Truncate from {} to {}", source.type(), destination.type());
        bit_extract(destination, source, 0, destination.type().width());
        return;
    }

    scalar truncate(const scalar& source, ir::value_type type) {
        auto destination = vreg_alloc_.allocate_scalar(type);
        truncate(destination, source);
        return destination;
    }

    instruction& branch(const label_operand& target) {
        label_refcount_[target.name()]++;
        return append(assembler::b(target).as_branch());
    }

    instruction& conditional_branch(const label_operand& target, const cond_operand& condition) {
        if (condition.condition() == cond_operand::conditions::eq) {
            label_refcount_[target.name()]++;
            return append(assembler::beq(target).as_branch());
        }

        if (condition.condition() == cond_operand::conditions::ne) {
            label_refcount_[target.name()]++;
            return append(assembler::bne(target).as_branch());
        }

        throw backend_exception("Cannot branch on condition {}", condition);
    }

    instruction& zero_compare_and_branch(const register_operand& source,
                                         const label_operand& target,
                                         const cond_operand& condition)
    {
        if (condition.condition() == cond_operand::conditions::eq) {
            label_refcount_[target.name()]++;
            return append(assembler::cbz(source, target));
        }

        if (condition.condition() == cond_operand::conditions::ne) {
            label_refcount_[target.name()]++;
            return append(assembler::cbnz(source, target));
        }

        throw backend_exception("Cannot zero-compare-and-branch on condition {}",
                                condition);
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

