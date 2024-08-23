#pragma once

#include <fmt/format.h>
#include <keystone/keystone.h>

#include <arancini/ir/value-type.h>
#include <arancini/output/dynamic/arm64/arm64-common.h>
#include <arancini/output/dynamic/machine-code-writer.h>

#include <array>
#include <cstdint>
#include <variant>
#include <type_traits>

namespace arancini::output::dynamic::arm64 {

class assembler {
public:
    assembler() {
        status_ = ks_open(KS_ARCH_ARM64, 0, &ks_);

        if (status_ != KS_ERR_OK)
            throw backend_exception("failed to initialise keystone assembler: {}", ks_strerror(status_));
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

static ir::value_type u12() {
    static ir::value_type type(ir::value_type_class::unsigned_integer, 12, 1);
    return type;
}

class register_operand {
public:
    enum regname64 : std::uint8_t {
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
        x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
        xzr_sp
    };

    enum regname32 : std::uint8_t {
        w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15,
        w16, w17, w18, w19, w20, w21, w22, w23, w24, w25, w26, w27, w28, w29, w30,
        wzr_sp
    };

    // NOTE: alias to regname_vector (32-bit LSB)
    enum regname_float32 : std::uint8_t {
        s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15,
        s16, s17, s18, s19, s20, s21, s22, s23, s24, s25, s26, s27, s28, s29, s30,
        s31
    };

    // NOTE: alias to regname_vector (64-bit LSB)
    enum regname_float64 : std::uint8_t {
        d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13, d14, d15,
        d16, d17, d18, d19, d20, d21, d22, d23, d24, d25, d26, d27, d28, d29, d30,
        d31
    };

    enum regname_vector: std::uint8_t {
        v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
        v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30,
        v31
    };

    enum special : std::uint8_t {
        nzcv
    };

    using regname = std::uint8_t;

    using register_index_type = std::size_t;

    register_operand() = default;

    explicit register_operand(regname32 reg): type_(ir::value_type::u32()), index_(reg) { }

    explicit register_operand(regname64 reg): type_(ir::value_type::u64()), index_(reg) { }

    explicit register_operand(regname_float32 reg): type_(ir::value_type::f32()), index_(reg) { }

    explicit register_operand(regname_float64 reg): type_(ir::value_type::f64()), index_(reg) { }

    explicit register_operand(special reg): special_(true), type_(ir::value_type::u64()), index_(reg) { }

    register_operand(register_index_type index, ir::value_type type):
        type_(type),
        index_(index)
    { }

    [[nodiscard]]
    bool is_special() const { return special_; }

    [[nodiscard]]
    bool is_virtual() const { return index_ > 32; }

    [[nodiscard]]
    ir::value_type type() const { return type_; }

    [[nodiscard]]
    register_index_type index() const { return index_; }
private:
    bool special_ = false;

    ir::value_type type_;
    register_index_type index_;
};

// TODO: ARM uses logical immediates that make determining their encoding completely different than
// what fits() does
class immediate_operand {
public:
    immediate_operand() = default;

    using value_type = arancini::ir::value_type;

    template <typename T, typename std::enable_if<std::is_arithmetic_v<T>, int>::type = 0>
	immediate_operand(T v, value_type type)
		: value_(v)
        , type_(type)
	{
        if (type_.is_vector() || type_.element_width() > 64)
            throw backend_exception("cannot represent vector {} as immediate", type_);

        if (!fits(v, type))
            throw backend_exception("specified immediate {} does not fit in width {}", v, type_.width());
	}

    [[nodiscard]]
    static bool fits(std::uintmax_t v, value_type type) {
        return type.element_width() == 64 || (v & ((1llu << type.element_width()) - 1)) == v;
    }

    [[nodiscard]]
    std::uintmax_t value() const { return value_; }

    [[nodiscard]]
    value_type &type() { return type_; }

    [[nodiscard]]
    const value_type &type() const { return type_; }
private:
    std::uintmax_t value_;
    value_type type_;
};

class shift_operand : public immediate_operand {
public:
    shift_operand() = default;

    shift_operand(const std::string &modifier, immediate_operand imm)
        : immediate_operand(imm)
        , modifier_(modifier)
    { }

    [[nodiscard]]
    std::string& modifier() { return modifier_; }

    [[nodiscard]]
    const std::string& modifier() const { return modifier_; }
private:
    std::string modifier_;
};

class label_operand {
public:
    label_operand() = default;

    label_operand(const std::string label):
        name_(label)
    { }

    [[nodiscard]]
    std::string& name() { return name_; }

    [[nodiscard]]
    const std::string& name() const { return name_; }
private:
    std::string name_;
};

class cond_operand {
public:
    cond_operand() = default;

    cond_operand(const std::string &cond):
        cond_(cond)
    { }

    [[nodiscard]]
    std::string& condition() { return cond_; }

    [[nodiscard]]
    const std::string& condition() const { return cond_; }
private:
    std::string cond_;
};

// TODO: replace indexing with enum
class memory_operand {
public:
    memory_operand() = default;

    enum class address_mode {
        direct,
        pre_index,
        post_index
    };

    template<typename T>
	memory_operand(const T &base,
                   immediate_operand offset = immediate_operand(0, u12()),
                   address_mode mode = address_mode::direct)
        : reg_base_(base)
		, offset_(offset)
        , mode_(mode)
	{ }

    template <typename T>
    void set_base_reg(const T &op) { reg_base_ = op; }

    bool is_virtual() const { return reg_base_.is_virtual(); }
    bool is_physical() const { return !is_virtual(); }

    [[nodiscard]]
    register_operand &base_register() { return reg_base_; }

    [[nodiscard]]
    const register_operand &base_register() const { return reg_base_; }

    [[nodiscard]]
    ir::value_type base_reg_type() const { return reg_base_.type(); }

    [[nodiscard]]
    immediate_operand offset() const { return offset_; }

    [[nodiscard]]
    address_mode mode() const { return mode_; }
private:
    register_operand reg_base_;

	immediate_operand offset_ = immediate_operand(0, u12());

    address_mode mode_;
};

enum class operand_type : std::uint8_t { invalid, reg, mem, imm, shift, label, cond};

struct operand {
    using operand_variant = std::variant<std::monostate,
                                         register_operand,
                                         memory_operand,
                                         immediate_operand,
                                         shift_operand,
                                         label_operand,
                                         cond_operand>;

    operand() = default;

    template <typename T>
    operand (const T &o)
          : op_(o)
          , use_(false)
          , def_(false)
          , keep_(false)
    {
    }

    operand_type type() const {
        return static_cast<operand_type>(op_.index());
    }

	bool is_reg() const { return type() == operand_type::reg; }
	bool is_mem() const { return type() == operand_type::mem; }
	bool is_imm() const { return type() == operand_type::imm; }
    bool is_shift() const { return type() == operand_type::shift; }
    bool is_cond() const { return type() == operand_type::cond; }
    bool is_label() const { return type() == operand_type::label; }

    register_operand &reg() { return std::get<register_operand>(op_); }
    const register_operand &reg() const { return std::get<register_operand>(op_); }

    memory_operand &memory() { return std::get<memory_operand>(op_); }
    const memory_operand &memory() const { return std::get<memory_operand>(op_); }

    immediate_operand &immediate() { return std::get<immediate_operand>(op_); }
    const immediate_operand &immediate() const { return std::get<immediate_operand>(op_); }

    shift_operand &shift() { return std::get<shift_operand>(op_); }
    const shift_operand &shift() const { return std::get<shift_operand>(op_); }

    cond_operand &cond() { return std::get<cond_operand>(op_); }
    const cond_operand &cond() const { return std::get<cond_operand>(op_); }

    label_operand &label() { return std::get<label_operand>(op_); }
    const label_operand &label() const { return std::get<label_operand>(op_); }

	bool is_use() const { return use_; }
	bool is_def() const { return def_; }
    bool is_keep() const { return keep_; }
	bool is_usedef() const { return use_ && def_; }

    void set_use() { use_ = true; }
    void set_def() { def_ = true; }
    void set_keep() { keep_ = true; }
    void set_usedef() { set_use(); set_def(); }

    // TODO: why is this needed?
    std::size_t width() const {
        switch (type()) {
        case operand_type::reg:
            return reg().type().width();
        case operand_type::mem:
            return memory().base_reg_type().width();
        case operand_type::imm:
            return immediate().type().width();
        default:
            return 0;
        }
    }

	void allocate(int index, ir::value_type value_type) {
		if (type() != operand_type::reg && reg().is_virtual())
			throw backend_exception("trying to allocate non-vreg");

		op_ = register_operand(index, value_type);
	}

	void allocate_base(int index, ir::value_type value_type) {
		if (type() != operand_type::mem)
			throw backend_exception("trying to allocate non-mem");

        auto &memory = std::get<memory_operand>(op_);
		if (!memory.is_virtual())
			throw backend_exception("trying to allocate non-virtual membase ");

        memory.set_base_reg(register_operand(index, value_type));
	}
protected:
    operand_variant op_;
	bool use_, def_, keep_;
};

template <typename T>
static operand def(const T &o)
{
    operand r = o;
    r.set_def();
    return r;
}

template <typename T>
static operand use(const T &o)
{
    operand r = o;
    r.set_use();
    return r;
}

template <typename T>
static operand keep(const T &o)
{
    operand r = o;
    r.set_keep();
    return r;
}

template <typename T>
static operand usedef(const T &o)
{
    operand r = o;
    r.set_usedef();
    return r;
}

class instruction {
public:
    typedef std::array<operand, 5> operand_array;

    template <typename... Args>
    instruction(const std::string& opc, Args&&... args)
        : opcode_(opc)
    {
        static_assert(sizeof...(Args) <= 5,
                      "aarch64 instructions accept at most 5 operands");
        opcount_ = sizeof...(Args);
        operands_ = {std::forward<Args>(args)...};
    }

    instruction(const label_operand &label)
        : opcode_(label.name())
        , label_(true)
    { }

    instruction &add_comment(const std::string &comment) { comment_ = comment; return *this; }

    [[nodiscard]]
    std::string& comment() { return comment_; }

    [[nodiscard]]
    const std::string& comment() const { return comment_; }

    instruction &set_branch(bool is_branch) { branch_ = is_branch; return *this; }

	void kill() { dead_ = true; }

    [[nodiscard]]
	bool is_dead() const { return dead_; }

    [[nodiscard]]
    bool is_branch() const { return branch_; }

    [[nodiscard]]
    bool is_label() const { return label_; }

    [[nodiscard]]
    std::string& opcode() { return opcode_; }

    [[nodiscard]]
    const std::string& opcode() const { return opcode_; }

    [[nodiscard]]
    std::size_t operand_count() const { return opcount_; }

    [[nodiscard]]
	operand_array &operands() { return operands_; }

    [[nodiscard]]
	const operand_array &operands() const { return operands_; }
private:
    std::string opcode_;
    std::string comment_;

    bool branch_ = false;
    bool label_ = false;
    bool dead_ = false;

    std::size_t opcount_ = 0;
    operand_array operands_;
};

} // namespace arancini::output::dynamic::arm64

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::register_operand> {
    template <typename FormatContext>
    constexpr auto parse(FormatContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(arancini::output::dynamic::arm64::register_operand op, FormatContext& ctx) const {
        using namespace arancini::output::dynamic::arm64;

        static const char* name64[] = {
            "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10",
            "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x19", "x20",
            "x21", "x22", "x23", "x24", "x25", "x26", "x27", "x28", "x29", "x30",
            "sp"
        };

        static const char* name32[] = {
            "w0", "w1", "w2", "w3", "w4", "w5", "w6", "w7", "w8", "w9", "w10",
            "w11", "w12", "w13", "w14", "w15", "w16", "w17", "w18", "w19", "w20",
            "w21", "w22", "w23", "w24", "w25", "w26", "w27", "w28", "w29", "w30",
            "sp"
        };

        static const char* name_float32[] = {
            "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10",
            "s11", "s12", "s13", "s14", "s15", "s16", "s17", "s18", "s19", "s20",
            "s21", "s22", "s23", "s24", "s25", "s26", "s27", "s28", "s29", "s30",
            "s31"
        };

        static const char* name_float64[] = {
            "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "d10",
            "d11", "d12", "d13", "d14", "d15", "d16", "d17", "d18", "d19", "d20",
            "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30",
            "d31"
        };

        static const char* name_special[] = {
            "nzcv"
        };

        // TODO: introduce check for NEON availability
        static const char* name_vector_neon[] = {
            "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10",
            "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20",
            "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30",
            "v31"
        };

        // TODO: introduce check for SVE2 availability
        static const char* name_vector_sve2[] = {
            "z0", "z1", "z2", "z3", "z4", "z5", "z6", "z7", "z8", "z9", "z10",
            "z11", "z12", "z13", "z14", "z15", "z16", "z17", "z18", "z19", "z20",
            "z21", "z22", "z23", "z24", "z25", "z26", "z27", "z28", "z29", "z30",
            "z31"
        };

        // TODO: vector predicates not used yet
        [[maybe_unused]]
        static const char* name_vector_pred[] = {
            "p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9", "p10",
            "p11", "p12", "p13", "p14", "p15"
        };

        if (op.is_virtual())
            return fmt::format_to(ctx.out(), "%V{}_{}", op.index(), op.type().element_width());

        if (op.is_special())
            return fmt::format_to(ctx.out(), "{}", name_special[op.index()]);

        std::string name;
        if (op.type().is_vector()) {
            name = op.type().width() == 128 ? name_vector_neon[op.index()] : name_vector_sve2[op.index()];
            switch (op.type().width()) {
            case 8:
                return fmt::format_to(ctx.out(), "{}.b", name);
            case 16:
                return fmt::format_to(ctx.out(), "{}.h", name);
            case 32:
                return fmt::format_to(ctx.out(), "{}.w", name);
            case 64:
                return fmt::format_to(ctx.out(), "{}.d", name);
            default:
                throw backend_exception("vectors larger than 64-bit not supported");
            }
        } else if (op.type().is_floating_point()) {
            name =  op.type().element_width() > 32 ? name_float64[op.index()] : name_float32[op.index()];
        } else if (op.type().is_integer()) {
            name =  op.type().element_width() > 32 ? name64[op.index()] : name32[op.index()];
        } else {
            throw backend_exception("physical registers are specified as 32-bit or 64-bit only");
        }

        return fmt::format_to(ctx.out(), "{}", name);
    }
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::memory_operand> {
    template <typename FormatContext>
    constexpr auto parse(FormatContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(arancini::output::dynamic::arm64::memory_operand mem, FormatContext& ctx) const {
        using namespace arancini::output::dynamic::arm64;
        using address_mode = memory_operand::address_mode;

        if (mem.is_virtual())
            fmt::format_to(ctx.out(), "[%V{}_{}",
                           mem.base_register().type().element_width(),
                           mem.base_register().index());
        else
            fmt::format_to(ctx.out(), "[{}", mem.base_register());

        if (!mem.offset().value()) {
            return fmt::format_to(ctx.out(), "]");
        }

        if (mem.mode() != address_mode::post_index)
            fmt::format_to(ctx.out(), ", #{:#x}]", mem.offset().value());
        else if (mem.mode() == address_mode::pre_index)
            fmt::format_to(ctx.out(), "!");

        if (mem.mode() == address_mode::post_index)
            fmt::format_to(ctx.out(), "], #{:#x}", mem.offset().value());

        // TODO: register indirect with index
        return ctx.out();
    }
};

// TODO: maybe this should inherit from the formatter for std::string_view (to get formatting)?
template <>
struct fmt::formatter<arancini::output::dynamic::arm64::operand> {
    template <typename FormatContext>
    constexpr auto parse(FormatContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(arancini::output::dynamic::arm64::operand op, FormatContext& ctx) const {
        using namespace arancini::output::dynamic::arm64;

        // TODO: should just be an std::visit() call
        switch (op.type()) {
        case operand_type::cond:
            // TODO: rename
            return fmt::format_to(ctx.out(), "{}", op.cond().condition());
        case operand_type::label:
            return fmt::format_to(ctx.out(), "{}", op.label().name());
        case operand_type::shift:
            if (!op.shift().modifier().empty())
                fmt::format_to(ctx.out(), "{} ", op.shift().modifier());
            return fmt::format_to(ctx.out(), "#{:#x}", op.shift().value());
        case operand_type::imm:
            return fmt::format_to(ctx.out(), "#{:#x}", op.immediate().value());
        case operand_type::mem:
            return fmt::format_to(ctx.out(), "{}", op.memory());
        case operand_type::reg:
            return fmt::format_to(ctx.out(), "{}", op.reg());
        default:
            // TODO: specify which
            throw backend_exception("attempting to format unknown operand type");
        }
    }
};

template <>
struct fmt::formatter<arancini::output::dynamic::arm64::instruction> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(arancini::output::dynamic::arm64::instruction instr, FormatContext& ctx) const {
        if (instr.is_dead())
            fmt::format_to(ctx.out(), "// Dead instruction: {}", instr.opcode());
        else
            fmt::format_to(ctx.out(), "{}", instr.opcode());

        if (instr.operand_count() == 0) {
            if (!instr.comment().empty())
                return fmt::format_to(ctx.out(), " // {}", instr.comment());

            return ctx.out();
        }

        const auto& operands = instr.operands();
        for (std::size_t i = 0; i < instr.operand_count() - 1; ++i) {
            fmt::format_to(ctx.out(), " {},", operands[i]);
        }

        fmt::format_to(ctx.out(), " {}", operands[instr.operand_count() - 1]);

        if (!instr.comment().empty())
            return fmt::format_to(ctx.out(), " // {}", instr.comment());

        return ctx.out();
    }
};

