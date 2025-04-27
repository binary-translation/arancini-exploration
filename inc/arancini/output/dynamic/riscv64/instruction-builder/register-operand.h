#pragma once

#include <cstdint>
namespace arancini::output::dynamic::riscv64::builder {
class RegisterOperand {
  public:
    static constexpr const uint32_t VIRTUAL_BASE = 64;
    static constexpr const uint32_t FUNCTIONAL_BASE = 32;

    explicit constexpr RegisterOperand(uint32_t encoding)
        : encoding_(encoding) {}
    constexpr RegisterOperand(Register reg) : encoding_(reg.encoding()) {}
    [[nodiscard]] bool is_virtual() const { return encoding_ >= VIRTUAL_BASE; }

    /// 32-63 are functional registers (e.g. 32-47 the 16 mapped GPRs, 63
    /// unused)

    [[nodiscard]] bool is_functional() const {
        return encoding_ < VIRTUAL_BASE && encoding_ >= FUNCTIONAL_BASE;
    }

    [[nodiscard]] bool is_physical() const {
        return encoding_ < FUNCTIONAL_BASE;
    }

    operator Register() const { return Register{encoding_}; }

    [[nodiscard]] uint32_t encoding() const { return encoding_; }

    constexpr bool operator==(const RegisterOperand &rhs) const {
        return encoding_ == rhs.encoding_;
    }
    constexpr bool operator!=(const RegisterOperand &rhs) const {
        return encoding_ != rhs.encoding_;
    }

    constexpr explicit operator bool() const;

    void allocate(uint32_t encoding) {
        if (is_physical()) {
            throw std::runtime_error("trying to allocate non-vreg");
        }
        if (encoding > 31) {
            throw std::runtime_error(
                "Allocating unavailable register at index: " +
                std::to_string(encoding));
        }
        encoding_ = encoding;
    }

    void set_encoding(uint32_t encoding) { encoding_ = encoding; }

  private:
    uint32_t encoding_;
};

constexpr const RegisterOperand none_reg{RegisterOperand::VIRTUAL_BASE - 1};

constexpr RegisterOperand::operator bool() const { return *this != none_reg; }

} // namespace arancini::output::dynamic::riscv64::builder