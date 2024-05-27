#pragma once

#include <keystone/keystone.h>

#include <arancini/output/dynamic/arm64/arm64-common.h>

#include <arancini/ir/value-type.h>
#include <arancini/util/logger.h>
#include <arancini/util/static-map.h>
#include <arancini/util/type-utils.h>
#include <arancini/output/dynamic/machine-code-writer.h>

#include <vector>
#include <variant>
#include <cstdint>
#include <type_traits>

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace arancini::output::dynamic::arm64 {

class assembler {
public:
    assembler() {
        status_ = ks_open(KS_ARCH_ARM64, 0, &ks_);
        if (status_ != KS_ERR_OK) {
            std::string msg("Failed to initialise keystone assembler: ");
            throw std::runtime_error(msg + ks_strerror(status_));
        }
    }

    size_t assemble(const char *code, unsigned char **out);

    void free(unsigned char* ptr) const { ks_free(ptr); }

    ~assembler() {
        ks_close(ks_);
    }
private:
    ks_err status_;
    ks_engine* ks_;
};

static assembler asm_;

static constexpr ir::value_type u12() {
    return ir::value_type(ir::value_type_class::unsigned_integer, 12, 1);
}

static constexpr ir::value_type u8() {
    return ir::value_type(ir::value_type_class::unsigned_integer, 8, 1);
}

// TODO: decide if this should auto-convert to 32-bit when given < 32-bit types
//
// Reasons to auto-convert:
// 1. Simplified construction directly from IR-provided value_type
// 2. There exist operations on u8/u16 representable in ARM that can use such registers (aliases to
// u32)
// 3. Upper-bits don't matter for point 2 instructions
// 4. Can handle casting properly via provided type
// 5. Sometimes avoids the need for manually casting between possibly incompatible register sizes
//
// Reason to not auto-convert
// 1. May introduce weird bugs that auto-cast 8-bit/16-bit values to larger sizes
class register_operand final {
public:
    using register_index = std::size_t;
    using register_type = ir::value_type;

    static constexpr register_index physical_register_count = 32;

    [[nodiscard]]
    constexpr bool is_special() const { return special_; }

    [[nodiscard]]
    constexpr std::size_t index() const { return index_; }

    [[nodiscard]]
    constexpr ir::value_type type() const { return type_; }

    [[nodiscard]]
    constexpr bool is_virtual() const { return index_ >= virtual_reg_start; }

    enum register_names_64bit : std::size_t {
        x0,
        x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
        x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
        xzr_sp
    };

    enum register_names_32bit : std::size_t {
        w0,
        w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15,
        w16, w17, w18, w19, w20, w21, w22, w23, w24, w25, w26, w27, w28, w29, w30,
        wzr_wsp
    };

    enum special_register_names : std::size_t {
        nzcv
    };

    // NOTE: alias to register_names_neon (32-bit LSB)
    enum register_names_float_32bit : std::size_t {
        s0,
        s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15,
        s16, s17, s18, s19, s20, s21, s22, s23, s24, s25, s26, s27, s28, s29, s30,
        s31
    };

    // NOTE: alias to register_names_neon (64-bit LSB)
    enum register_names_float_64bit : std::size_t {
        d0,
        d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13, d14, d15,
        d16, d17, d18, d19, d20, d21, d22, d23, d24, d25, d26, d27, d28, d29, d30,
        d31
    };

    enum register_names_neon : std::size_t {
        v0,
        v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
        v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30,
        v31
    };

    explicit register_operand(register_names_64bit reg):
        register_operand(reg, ir::value_type::u64())
    { }

    explicit register_operand(register_names_32bit reg):
        register_operand(reg, ir::value_type::u32())
    { }

    explicit register_operand(register_names_float_64bit reg):
        register_operand(reg, ir::value_type::f64())
    { }

    explicit register_operand(register_names_float_32bit reg):
        register_operand(reg, ir::value_type::u32())
    { }

    explicit register_operand(register_names_neon reg):
        register_operand(reg, ir::value_type::u128())
    { }

    explicit register_operand(special_register_names reg):
        register_operand(reg, ir::value_type::u64())
    {
        special_ = true;
    }

    register_operand(register_index index, const ir::value_type &type):
        type_(type),
        index_(index)
    {
        static constexpr std::array<register_type, 9> supported_types_ = {
            register_type::u1(),
            register_type::s8(),
            register_type::u8(),
            register_type::u32(),
            register_type::s32(),
            register_type::u64(),
            register_type::s64(),
            register_type::f32(),
            register_type::f64()
        };

        if (!util::contains(supported_types_, type_)) {
            throw arm64_exception("Register defined with unsupported type {}\n", type_);
        }
    }
private:
    register_type type_;
    register_index index_;

    bool special_ = false;

    static constexpr register_index virtual_reg_start = physical_register_count;
};

inline bool operator==(const register_operand &op1, const register_operand &op2) {
    return op1.type() == op2.type() &&
           op1.index() == op2.index() &&
           op1.is_special() == op2.is_special();
}

inline bool operator!=(const register_operand &op1, const register_operand &op2) {
    return !(op1 == op2);
}

template <typename T>
constexpr ir::value_type_class convert_to_type_class() {
    static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>,
                  "Value types are defined only for integral and floating point values");

    static_assert(sizeof(T) <= 64,
                  "Value types are defined only for integral and floating point values");

    if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_unsigned_v<T>)
            return ir::value_type_class::unsigned_integer;

        if constexpr (std::is_signed_v<T>)
            return ir::value_type_class::signed_integer;
    }

    return ir::value_type_class::floating_point;
}

class immediate_operand {
public:
    using value_type = ir::value_type;
    using base_type  = std::uintmax_t;

    template <typename T>
	constexpr immediate_operand(T v, ir::value_type type)
        : type_{type}
		, value_{util::bitcast<decltype(value_)>(v)}
	{
        // Ensure we can represent the specified value
        convert_to_type_class<T>();

        if (type_.is_vector() || !type_.element_width() || type_.element_width() > 64 || !fits(v, type))
            throw arm64_exception("Cannot construct immediate_operand from value {} for value type {}\n",
                                  v, type);
	}

    [[nodiscard]]
    constexpr ir::value_type type() const { return type_; }

    [[nodiscard]]
    constexpr std::uintmax_t value() const { return value_ & ~(~0 << (type_.element_width()-1)); }

    [[nodiscard]]
    static constexpr bool fits(std::uintmax_t v, value_type type) {
        return type.element_width() == 64 || (v & ((1llu << type.element_width()) - 1)) == v;
    }
private:
    value_type type_;
    std::uintmax_t value_;
};

template <auto value, std::size_t size>
constexpr immediate_operand immediate() {
    // Ensure we can represent the specified value
    static_assert(immediate_operand::fits(value, ir::value_type{convert_to_type_class<decltype(value)>(), size}),
                  "Specified immediate operand value cannot be represented within specified size");

    return immediate_operand{util::bitcast<decltype(value)>(value),
                             ir::value_type{convert_to_type_class<decltype(value)>(), size}};
}

class shift_operand final {
public:
    shift_operand(std::string_view modifier, immediate_operand shift_amount)
        : modifier_(modifier)
        , amount_(shift_amount)
    {
    }

    [[nodiscard]]
    std::string_view modifier() const { return modifier_; }

    [[nodiscard]]
    immediate_operand amount() const { return amount_; }
private:
    std::string modifier_;
    immediate_operand amount_;
};

class label_operand final {
public:
    label_operand(std::string_view label):
        name_(label)
    { }

    [[nodiscard]]
    std::string_view name() const { return name_; }
private:
    std::string name_;
};

class conditional_operand final {
public:
    conditional_operand(std::string_view condition):
        condition_(condition)
    { }

    [[nodiscard]]
    std::string_view condition() const { return condition_; }
private:
    std::string condition_;
};

class memory_operand final {
public:
    // TODO: implement all addressing_modes
    enum class addressing_modes {
        direct,
        pre_index,
        post_index
    };

    using address_base = register_operand;

	memory_operand(const register_operand &base,
                   immediate_operand offset = immediate_operand(0, u12()),
                   addressing_modes addressing_mode = addressing_modes::direct)
        : base_register_(base)
		, offset_(offset)
        , addressing_mode_(addressing_mode)
	{
        if (base_register_.type().width() != 32 && base_register_.type().width() != 64)
            throw arm64_exception("Base register for memory access must"
                                  "be either 32-bits or 64-bits; register of type {} passed",
                                  base_register_.type());

        if (base_register_.type().type_class() == ir::value_type_class::floating_point ||
            base_register_.type().type_class() == ir::value_type_class::none) {
            throw arm64_exception("Base register type cannot be of type {}",
                                  base_register_.type());
        }
    }

    template <typename T>
    void set_base_register(const T &op) { base_register_ = op; }

    address_base &base_register() { return base_register_; }
    const address_base &base_register() const { return base_register_; }

    immediate_operand &offset() { return offset_; }
    const immediate_operand &offset() const { return offset_; }

    addressing_modes addressing_mode() const { return addressing_mode_; }
private:
    address_base base_register_;

	immediate_operand offset_;
    enum addressing_modes addressing_mode_;
};

template <typename T, ir::value_type_class V>
struct check_type_class {
    check_type_class(T &o) {
        if constexpr (std::is_same_v<T, register_operand>) {
            if (o.type().type_class() != V) std::abort();
        }

        if constexpr (std::is_same_v<T, memory_operand>) {
            if (o.base_register().type().type_class() != V) std::abort();
        }

        if constexpr (std::is_same_v<T, immediate_operand>) {
            if (o.type().type_class() != V) std::abort();
        }
    }

    using type = T;
};

template <typename T, std::size_t Size, typename Compare = std::equal_to<std::size_t>>
struct check_size {
    check_size(T &o) {
        if constexpr (std::is_same_v<T, register_operand>) {
            if (Compare(o.type().width(), Size)) std::abort();
        }

        if constexpr (std::is_same_v<T, memory_operand>) {
            if (Compare(o.base_register().type().width(), Size)) std::abort();
        }

        if constexpr (std::is_same_v<T, immediate_operand>) {
            if (Compare(o.type().width(), Size)) std::abort();
        }
    }

    using type = T;
};

template <typename T>
struct check_special {
    check_special(T &o) {
        if constexpr (std::is_same_v<T, register_operand>) {
            if (!o.is_special()) std::abort();
        }
    }
};

class operand final {
public:
    using base_type = std::variant<register_operand, immediate_operand,
                                   shift_operand, label_operand,
                                   conditional_operand, memory_operand>;

    template <typename Arg>
    operand(const Arg &arg): operand_(arg) { }

    template <typename... Args>
    operand(const std::variant<Args...> v): operand_(util::variant_cast(v)) { }

    base_type &get() { return operand_; }
    const base_type &get() const { return operand_; }

    void set_use() { use_ = true; }
    void set_def() { def_ = true; }

    bool is_use() const { return use_; }
    bool is_def() const { return def_; }
private:
    bool use_ = false;
    bool def_ = false;
    base_type operand_;
};

inline operand use(operand o) {
    o.set_use();
    return o;
}

inline operand def(operand o) {
    o.set_use();
    return o;
}

inline operand usedef(operand o) {
    o.set_use();
    o.set_def();
    return o;
}

class instruction {
public:
    template <typename... Args>
    instruction(const std::string &opcode, Args&&... operands):
        opcode_(opcode), operands_{operands...}
    { }

    // TODO: replace with something better
    using operand_array = std::vector<operand>;

    operand_array &operands() { return operands_; }
    const operand_array &operands() const { return operands_; }

    std::string_view opcode() const { return opcode_; }

    instruction &set_keep() { keep_ = true; return *this; }
    bool is_keep() const { return keep_; }

    instruction &set_copy() { copy_ = true; return *this; }
    bool is_copy() const { return copy_; }

    instruction &set_branch() { branch_ = true; return *this; }
    bool is_branch() const { return branch_; }

	bool is_dead() const { return opcode_.empty(); }

    instruction &comment(std::string_view comment) { comment_ = comment; return *this; }

	void kill() { opcode_.clear(); }

    std::string_view comment() const { return comment_; }

    virtual ~instruction() = default;
private:
    bool keep_ = false;
    bool copy_ = false;
    bool branch_ = false;

    std::string opcode_;
    operand_array operands_;

    std::string comment_;
};

} // namespace arancini::output::dynamic::arm64

// TODO: maybe should be in .cpp

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::register_operand> final : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::register_operand &reg, FCTX &format_ctx) const {
        if (reg.is_virtual())
            return fmt::format_to(format_ctx.out(), "%V{}_{}", reg.type().width(), reg.index());

        if (reg.is_special())
            return fmt::format_to(format_ctx.out(), "nzcv");

        std::string_view regname;
        if (reg.type().type_class() != arancini::ir::value_type_class::floating_point) {
            if (reg.type().width() <= 32)
                regname = r32_bit.at(reg.index());
            else
                regname = r64_bit.at(reg.index());
        } else {
            if (reg.type().width() <= 32)
                regname = f32_bit.at(reg.index());
            else
                regname = f64_bit.at(reg.index());
        }

        return fmt::format_to(format_ctx.out(), "{}", regname);
    }
private:
    // Assumes register operands have contiguous values
    using name_table = std::array<std::string_view, arancini::output::dynamic::arm64::register_operand::physical_register_count+1>;
    static constexpr name_table r64_bit {
        "x0",
        "x1",
        "x2",
        "x3",
        "x4",
        "x5",
        "x6",
        "x7",
        "x8",
        "x9",
        "x10",
        "x11",
        "x12",
        "x13",
        "x14",
        "x15",
        "x16",
        "x17",
        "x18",
        "x19",
        "x20",
        "x21",
        "x22",
        "x23",
        "x24",
        "x25",
        "x26",
        "x27",
        "x28",
        "x29",
        "x30",
        "sp",
    };

    static constexpr name_table r32_bit {
        "w0",
        "w1",
        "w2",
        "w3",
        "w4",
        "w5",
        "w6",
        "w7",
        "w8",
        "w9",
        "w10",
        "w11",
        "w12",
        "w13",
        "w14",
        "w15",
        "w16",
        "w17",
        "w18",
        "w19",
        "w20",
        "w21",
        "w22",
        "w23",
        "w24",
        "w25",
        "w26",
        "w27",
        "w28",
        "w29",
        "w30",
        "wsp"
    };

   static constexpr name_table f32_bit {
        "s0",
        "s1",
        "s2",
        "s3",
        "s4",
        "s5",
        "s6",
        "s7",
        "s8",
        "s9",
        "s10",
        "s11",
        "s12",
        "s13",
        "s14",
        "s15",
        "s16",
        "s17",
        "s18",
        "s19",
        "s20",
        "s21",
        "s22",
        "s23",
        "s24",
        "s25",
        "s26",
        "s27",
        "s28",
        "s29",
        "s30",
   };

   static constexpr name_table f64_bit {
       "d0",
       "d1",
       "d2",
       "d3",
       "d4",
       "d5",
       "d6",
       "d7",
       "d8",
       "d9",
       "d10",
       "d11",
       "d12",
       "d13",
       "d14",
       "d15",
       "d16",
       "d17",
       "d18",
       "d19",
       "d20",
       "d21",
       "d22",
       "d23",
       "d24",
       "d25",
       "d26",
       "d27",
       "d28",
       "d29",
       "d30"
  };

  static constexpr name_table r128_bit {
        "v0",
        "v1",
        "v2",
        "v3",
        "v4",
        "v5",
        "v6",
        "v7",
        "v8",
        "v9",
        "v10",
        "v11",
        "v12",
        "v13",
        "v14",
        "v15",
        "v16",
        "v17",
        "v18",
        "v19",
        "v20",
        "v21",
        "v22",
        "v23",
        "v24",
        "v25",
        "v26",
        "v27",
        "v28",
        "v29",
        "v30"
    };
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::memory_operand> final : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::memory_operand &mop, FCTX &format_ctx) const {
        using addressing_modes = enum arancini::output::dynamic::arm64::memory_operand::addressing_modes;

        fmt::format_to(format_ctx.out(), "[{}", mop.base_register());

        if (!mop.offset().value())
            return fmt::format_to(format_ctx.out(), "]");

        if (mop.addressing_mode() != addressing_modes::post_index)
            fmt::format_to(format_ctx.out(), ", #{:#x}]", mop.offset().value());
        else if (mop.addressing_mode() == addressing_modes::pre_index)
            fmt::format_to(format_ctx.out(), "!");

        if (mop.addressing_mode() == addressing_modes::post_index)
            fmt::format_to(format_ctx.out(), "], #{:#x}", mop.offset().value());

        return format_ctx.out();
    }
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::label_operand> final : public fmt::formatter<std::string_view> {
    template <typename PCTX> constexpr format_parse_context::iterator parse(const PCTX &parse_ctx) {
        return parse_ctx.begin();
    }

    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::label_operand &label, FCTX &format_ctx) const {
        return fmt::format_to(format_ctx.out(), "{}", label.name());
    }
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::shift_operand> final : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::shift_operand &shift, FCTX &format_ctx) const {
        if (!shift.modifier().empty())
            fmt::format_to(format_ctx.out(), "{} ", shift.modifier());
        return fmt::format_to(format_ctx.out(), "{}", shift.amount());
    }
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::conditional_operand> final : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::conditional_operand &conditional, FCTX &format_ctx) const {
        return fmt::format_to(format_ctx.out(), "{}", conditional.condition());
    }
};

// FIXME: this works; but it's not well implemented
// immediate_operand is just a wrapper over a value, it should format like the value that it wraps
template <>
struct fmt::formatter<arancini::output::dynamic::arm64::immediate_operand> final : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::immediate_operand &immediate, FCTX &format_ctx) const {
        return fmt::format_to(format_ctx.out(), "#{:#x}", immediate.value());
    }
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::operand> final : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::operand &operand, FCTX &format_ctx) const {
        return std::visit(
            [&format_ctx](const auto &op) {
                return fmt::format_to(format_ctx.out(), "{}", op);
            }, operand.get());
    }
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::instruction> final : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::instruction &insn, FCTX &format_ctx) const {
        // This causes a gap in the instruction stream to appear; might cause a small slowdown
        // TODO: investigate
        if (insn.is_dead()) return format_ctx.out();

        if (insn.comment().empty())
            return fmt::format_to(format_ctx.out(), "{} {}", insn.opcode(), fmt::join(insn.operands(), ", "));
        return fmt::format_to(format_ctx.out(), "{} {} // {}", insn.opcode(), fmt::join(insn.operands(), ", "), insn.comment());
    }
};

