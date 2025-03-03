#pragma once

#include <arancini/output/dynamic/arm64/arm64-instruction.h>

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

} // namespace arancini::ir::output::dynamic::arm64

