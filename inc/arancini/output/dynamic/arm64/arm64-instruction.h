#pragma once

#include <keystone/keystone.h>

#include <arancini/output/dynamic/arm64/arm64-common.h>

#include <arancini/ir/value-type.h>
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
        wzr_sp
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
        if (std::find(supported_types_.begin(), supported_types_.end(), type_) == supported_types_.end()) {
            // TODO: change to custom exception type
            throw arm64_exception("Register defined with unsupported type {}\n", type_);
        }
    }
private:
    register_type type_;
    register_index index_;

    bool special_ = false;

    static constexpr register_index virtual_reg_start = physical_register_count;

    // TODO: make constexpr
    static std::array<register_type, 4> supported_types_;
};

class immediate_operand {
public:
    using value_type = ir::value_type;
    using base_type  = std::uintmax_t;

    template <typename T, typename std::enable_if<std::is_arithmetic_v<T>, int>::type = 0>
	constexpr immediate_operand(T v, ir::value_type type)
        : type_(type)
		, value_(v)
	{
        if (type_.is_vector() || type_.element_width() > 64)
            std::abort();

        if (!fits(v, type))
            std::abort();
	}

    [[nodiscard]]
    constexpr ir::value_type type() const { return type_; }

    [[nodiscard]]
    constexpr std::uintmax_t value() const { return value_; }

    [[nodiscard]]
    static constexpr bool fits(uintmax_t v, value_type type) {
        return type.element_width() == 64 || (v & ((1llu << type.element_width()) - 1)) == v;
    }
private:
    value_type type_;
    std::uintmax_t value_;
};

template <std::size_t Size, ir::value_type_class TypeClass>
struct immediate_t : private immediate_operand {
    template <typename T>
    immediate_t(T value): immediate_operand(value, ir::value_type(TypeClass, Size, 1))
    {
        constexpr std::intmax_t v = value;
        static_assert(Size == 64 || (v & ((1llu << 64) - 1)) == v, 
                      "immediate does not fit within required size");
    }
};

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

    instruction &set_branch() { branch_ = true; return *this; }
    bool is_branch() const { return branch_; }

	bool is_dead() const { return opcode_.empty(); }

    instruction &comment(std::string_view comment) { comment_ = comment; return *this; }

	void kill() { opcode_.clear(); }

    std::string_view comment() const { return comment_; }

    virtual ~instruction() = default;
private:
    bool keep_ = false;
    bool branch_ = false;

    std::string opcode_;
    operand_array operands_;

    std::string comment_;
};

} // namespace arancini::output::dynamic::arm64

// TODO: maybe should be in .cpp

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::register_operand> : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::register_operand &reg, FCTX &format_ctx) const {
        #define MATCH(name) std::make_pair(arancini::output::dynamic::arm64::register_operand::name, #name)

        using name_table = util::static_map<arancini::output::dynamic::arm64::register_operand::register_index, 
                                            const char *, arancini::output::dynamic::arm64::register_operand::physical_register_count+1>;

        static util::static_map<arancini::ir::value_type, name_table, 5> names {
            // 64-bit scalar registers
            std::make_pair(arancini::ir::value_type::u64(), 
                name_table {
                    MATCH(x0),
                    MATCH(x0),
                    MATCH(x1),
                    MATCH(x2),
                    MATCH(x3),
                    MATCH(x4),
                    MATCH(x5),
                    MATCH(x6),
                    MATCH(x7),
                    MATCH(x8),
                    MATCH(x9),
                    MATCH(x10),
                    MATCH(x11),
                    MATCH(x12),
                    MATCH(x13),
                    MATCH(x14),
                    MATCH(x15),
                    MATCH(x16),
                    MATCH(x17),
                    MATCH(x18),
                    MATCH(x19),
                    MATCH(x20),
                    MATCH(x21),
                    MATCH(x22),
                    MATCH(x23),
                    MATCH(x24),
                    MATCH(x25),
                    MATCH(x26),
                    MATCH(x27),
                    MATCH(x28),
                    MATCH(x29),
                    MATCH(x30),
                    std::make_pair(arancini::output::dynamic::arm64::register_operand::xzr_sp, "sp")
                }),

            
            // 32-bit scalar registers
            std::make_pair(arancini::ir::value_type::u32(), 
                name_table {
                MATCH(w0),
                MATCH(w1),
                MATCH(w2),
                MATCH(w3),
                MATCH(w4),
                MATCH(w5),
                MATCH(w6),
                MATCH(w7),
                MATCH(w8),
                MATCH(w9),
                MATCH(w10),
                MATCH(w11),
                MATCH(w12),
                MATCH(w13),
                MATCH(w14),
                MATCH(w15),
                MATCH(w16),
                MATCH(w17),
                MATCH(w18),
                MATCH(w19),
                MATCH(w20),
                MATCH(w21),
                MATCH(w22),
                MATCH(w23),
                MATCH(w24),
                MATCH(w25),
                MATCH(w26),
                MATCH(w27),
                MATCH(w28),
                MATCH(w29),
                MATCH(w30),
                std::make_pair(arancini::output::dynamic::arm64::register_operand::wzr_sp, "sp")
            }),

            // Double precision floating-point
            std::make_pair(arancini::ir::value_type::f64(), 
                name_table {
                    MATCH(d0),
                    MATCH(d1),
                    MATCH(d2),
                    MATCH(d3),
                    MATCH(d4),
                    MATCH(d5),
                    MATCH(d6),
                    MATCH(d7),
                    MATCH(d8),
                    MATCH(d9),
                    MATCH(d10),
                    MATCH(d11),
                    MATCH(d12),
                    MATCH(d13),
                    MATCH(d14),
                    MATCH(d15),
                    MATCH(d16),
                    MATCH(d17),
                    MATCH(d18),
                    MATCH(d19),
                    MATCH(d20),
                    MATCH(d21),
                    MATCH(d22),
                    MATCH(d23),
                    MATCH(d24),
                    MATCH(d25),
                    MATCH(d26),
                    MATCH(d27),
                    MATCH(d28),
                    MATCH(d29),
                    MATCH(d30)
            }),
        
            // Single precision floating-point
            std::make_pair(arancini::ir::value_type::f32(), 
                name_table {
                    MATCH(s0),
                    MATCH(s1),
                    MATCH(s2),
                    MATCH(s3),
                    MATCH(s4),
                    MATCH(s5),
                    MATCH(s6),
                    MATCH(s7),
                    MATCH(s8),
                    MATCH(s9),
                    MATCH(s10),
                    MATCH(s11),
                    MATCH(s12),
                    MATCH(s13),
                    MATCH(s14),
                    MATCH(s15),
                    MATCH(s16),
                    MATCH(s17),
                    MATCH(s18),
                    MATCH(s19),
                    MATCH(s20),
                    MATCH(s21),
                    MATCH(s22),
                    MATCH(s23),
                    MATCH(s24),
                    MATCH(s25),
                    MATCH(s26),
                    MATCH(s27),
                    MATCH(s28),
                    MATCH(s29),
                    MATCH(s30),
            }),

            // NEON registers
            // should be added only on compile-time switch
            std::make_pair(arancini::ir::value_type::u128(), 
                name_table {
                    MATCH(v0),
                    MATCH(v1),
                    MATCH(v2),
                    MATCH(v3),
                    MATCH(v4),
                    MATCH(v5),
                    MATCH(v6),
                    MATCH(v7),
                    MATCH(v8),
                    MATCH(v9),
                    MATCH(v10),
                    MATCH(v11),
                    MATCH(v12),
                    MATCH(v13),
                    MATCH(v14),
                    MATCH(v15),
                    MATCH(v16),
                    MATCH(v17),
                    MATCH(v18),
                    MATCH(v19),
                    MATCH(v20),
                    MATCH(v21),
                    MATCH(v22),
                    MATCH(v23),
                    MATCH(v24),
                    MATCH(v25),
                    MATCH(v26),
                    MATCH(v27),
                    MATCH(v28),
                    MATCH(v29),
                    MATCH(v30)
                }),
        };

        if (reg.is_virtual()) 
            return fmt::format_to(format_ctx.out(), "%V{}_{}", reg.type().width(), reg.index());

        if (reg.is_special())
            return fmt::format_to(format_ctx.out(), "nzcv");

        return fmt::format_to(format_ctx.out(), "{}", names.at(reg.type()).at(reg.index()));
    }
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::memory_operand> : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::memory_operand &mop, FCTX &format_ctx) const {
        using addressing_modes = enum arancini::output::dynamic::arm64::memory_operand::addressing_modes; 

        fmt::format_to(format_ctx.out(), "[{}", mop.base_register());

        if (!mop.offset().value()) 
            fmt::format_to(format_ctx.out(), "]");

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
struct fmt::formatter<arancini::output::dynamic::arm64::label_operand> : public fmt::formatter<std::string_view> {
    template <typename PCTX> constexpr format_parse_context::iterator parse(const PCTX &parse_ctx) {
        return parse_ctx.begin();
    }

    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::label_operand &label, FCTX &format_ctx) const {
        return fmt::format_to(format_ctx.out(), "{}", label.name());
    }
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::shift_operand> : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::shift_operand &shift, FCTX &format_ctx) const {
        if (!shift.modifier().empty())
            fmt::format_to(format_ctx.out(), "{} ", shift.modifier());
        return fmt::format_to(format_ctx.out(), "{:#x}", shift.amount());
    }
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::conditional_operand> : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::conditional_operand &conditional, FCTX &format_ctx) const {
        return fmt::format_to(format_ctx.out(), "{}", conditional.condition());
    }
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::immediate_operand> : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::immediate_operand &immediate, FCTX &format_ctx) const {
        return fmt::format_to(format_ctx.out(), "{:#x}", immediate.value());
    }
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::operand> : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::operand &operand, FCTX &format_ctx) const {
        return std::visit(
            [&format_ctx](const auto &op) {
                return fmt::format_to(format_ctx.out(), "{}", op);
            }, operand.get());
    }
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::instruction::operand_array> : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::instruction::operand_array &op_array, FCTX &format_ctx) const {
        // TODO: is this correct?
        if (op_array.size() == 0) return format_ctx.out();

        for (size_t i = 0; i < op_array.size() - 1; ++i) {
            fmt::format_to(format_ctx.out(), "{}, ", op_array[i]);
        }

        return fmt::format_to(format_ctx.out(), "{}", op_array[op_array.size()-1]);
    }
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::instruction> : public fmt::formatter<std::string_view> {
    template <typename FCTX>
    format_context::iterator format(const arancini::output::dynamic::arm64::instruction &insn, FCTX &format_ctx) const {
        if (insn.comment().empty())
            return fmt::format_to(format_ctx.out(), "{} {}", insn.opcode(), insn.operands());
        return fmt::format_to(format_ctx.out(), "{} {} // {}", insn.opcode(), insn.operands(), insn.comment());
    }
};

