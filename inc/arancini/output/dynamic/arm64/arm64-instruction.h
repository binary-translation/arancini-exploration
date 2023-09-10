#pragma once

#include <cstdint>
#include <initializer_list>
#include <keystone/keystone.h>

#include <arancini/ir/value-type.h>
#include <arancini/output/dynamic/machine-code-writer.h>

#include <array>
#include <type_traits>
#include <vector>
#include <variant>
#include <sstream>
#include <iostream>
#include <stdexcept>

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

static ir::value_type u12() {
    static ir::value_type type(ir::value_type_class::unsigned_integer, 12, 1);
    return type;
}

class vreg_operand {
public:
    vreg_operand() = default;

    using value_type = arancini::ir::value_type;

	vreg_operand(unsigned int i, arancini::ir::value_type type)
		: index_(i)
        , type_(type)
	{
	}

    vreg_operand(const vreg_operand &o)
        : index_(o.index_)
        , type_(o.type_)
    {
    }

    vreg_operand &operator=(const vreg_operand &o) {
        type_ = o.type_;
        index_ = o.index_;

        return *this;
    }

    value_type &type() { return type_; }
    const value_type &type() const { return type_; }

    size_t width() const { return type_.element_width(); }

    unsigned int index() const { return index_; }
private:
	unsigned int index_;
    value_type type_;
};

class preg_operand {
public:
    enum regname64 : uint8_t {
        none = 0,
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
        x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30,
        xzr_sp
    };

    enum regname32 : uint8_t {
        none32 = 0,
        w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15,
        w16, w17, w18, w19, w20, w21, w22, w23, w24, w25, w26, w27, w28, w29, w30,
        wzr_sp
    };

    // NOTE: alias to regname_vector (32-bit LSB)
    enum regname_float32 : uint8_t {
        none_float32 = 0,
        s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14, s15,
        s16, s17, s18, s19, s20, s21, s22, s23, s24, s25, s26, s27, s28, s29, s30,
        s31
    };

    // NOTE: alias to regname_vector (64-bit LSB)
    enum regname_float64 : uint8_t {
        none_float64 = 0,
        d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13, d14, d15,
        d16, d17, d18, d19, d20, d21, d22, d23, d24, d25, d26, d27, d28, d29, d30,
        d31
    };

    enum regname_vector: uint8_t {
        none_vector = 0,
        v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
        v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30,
        v31
    };

    enum special : uint8_t {
        none_special = 0,
        nzcv
    };

    using regname = uint8_t;

    preg_operand() = default;

    explicit preg_operand(regname32 reg): type_(ir::value_type::u32()), index_(reg) { }

    explicit preg_operand(regname64 reg): type_(ir::value_type::u64()), index_(reg) { }

    explicit preg_operand(regname_float32 reg): type_(ir::value_type::f32()), index_(reg) { }

    explicit preg_operand(regname_float64 reg): type_(ir::value_type::f64()), index_(reg) { }

    explicit preg_operand(special reg): special_(true), type_(ir::value_type::u64()), index_(reg) { }

    explicit preg_operand(size_t index, ir::value_type type) {
        type_ = type;
        index_ = index+1;
        if (index > static_cast<size_t>(xzr_sp) + 1)
            throw std::runtime_error("Allocating unavailable register at index: "
                                     + std::to_string(index));
    }

    preg_operand(const preg_operand& r)
        : special_(r.special_)
        , type_(r.type_)
        , index_(r.index_)
    { }

    preg_operand& operator=(const preg_operand& r) {
        index_ = r.index_;
        type_ = r.type_;
        special_ = r.special_;
        return *this;
    }

    bool special() const { return special_; }

    ir::value_type type() const { return type_; }

    size_t width() const { return type_.element_width();; }

    size_t register_index() const { return index_; }
private:
    bool special_ = false;
    ir::value_type type_;

    size_t index_;
};

std::string to_string(const preg_operand&);

// TODO: how are immediates represented in arm
// TODO: fix this
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
            throw std::runtime_error("Cannot represent vectors as immediates");

        if (!fits(v, type))
            throw std::runtime_error("Specified immediate does not fit in width: " +
                                     std::to_string(width()) + " " + std::to_string(v));
	}

    static bool fits(uintmax_t v, value_type type) {
        return type.element_width() == 64 || (v & ((1llu << type.element_width()) - 1)) == v;
    }

    size_t width() const { return type_.element_width(); }
    uintmax_t value() const { return value_; }

    value_type &type() { return type_; }
    const value_type &type() const { return type_; }
private:
    uintmax_t value_;
    value_type type_;

};

class shift_operand : public immediate_operand {
public:
    // TODO: check fit in 48-bit shift specifier
    shift_operand() = default;

    shift_operand(const std::string &modifier, size_t amount = 0, value_type type = value_type::u8())
        : immediate_operand(amount, type)
    {
        modifier_ = modifier;
    }

    std::string& modifier() { return modifier_; }
    const std::string& modifier() const { return modifier_; }
private:
    std::string modifier_;
};

class label_operand {
public:
    label_operand() = default;

    label_operand(const std::string &label):
        name_(label)
    {
    }


    std::string& name() { return name_; }
    const std::string& name() const { return name_; }
private:
    std::string name_;
};

class cond_operand {
public:
    cond_operand() = default;

    cond_operand(const std::string &cond): cond_(cond)
    {
    }

    cond_operand(const cond_operand &c): cond_(c.cond_) { }

    std::string& condition() { return cond_; }
    const std::string& condition() const { return cond_; }
private:
    std::string cond_;
};

class memory_operand {
public:
    memory_operand() = default;

    template<typename T>
	memory_operand(const T &base,
                   immediate_operand offset = immediate_operand(0, u12()),
                   bool pre_index = false,
                   bool post_index = false)
        : reg_base_(base)
		, offset_(offset)
        , pre_index_(pre_index)
        , post_index_(post_index)
	{
        if (pre_index == post_index && pre_index)
            throw std::runtime_error("Both pre- and post-index passed to ARM DBT");
    }

    memory_operand(const memory_operand& m)
        : reg_base_(m.reg_base_)
        , offset_(m.offset_)
        , pre_index_(m.pre_index_)
        , post_index_(m.post_index_)
    {
    }

    memory_operand& operator=(const memory_operand& m) {
        reg_base_ = m.reg_base_;
        offset_ = m.offset_;
        pre_index_ = m.pre_index_;
        post_index_ = m.post_index_;

        return *this;
    }

    template <typename T>
    void set_base_reg(const T &op) { reg_base_ = op; }

    bool is_virtual() const { return reg_base_.index() == 0; }
    bool is_physical() const { return !is_virtual(); }

    vreg_operand &vreg_base() { return std::get<vreg_operand>(reg_base_); }
    const vreg_operand &vreg_base() const { return std::get<vreg_operand>(reg_base_); }

    preg_operand &preg_base() { return std::get<preg_operand>(reg_base_); }
    const preg_operand &preg_base() const { return std::get<preg_operand>(reg_base_); }

    size_t base_width() const {
        if (is_virtual())
            return std::get<vreg_operand>(reg_base_).width();
        return std::get<preg_operand>(reg_base_).width();
    }

    immediate_operand offset() const { return offset_; }
    bool pre_index() const { return pre_index_; }
    bool post_index() const { return post_index_; }
private:
    std::variant<vreg_operand, preg_operand> reg_base_;

	immediate_operand offset_ = immediate_operand(0, u12());
    bool pre_index_ = false;
    bool post_index_ = false;

};

enum class operand_type : uint8_t { invalid, preg, vreg, mem, imm, shift, label, cond};

struct operand {
    using operand_variant = std::variant<std::monostate,
                                         preg_operand,
                                         vreg_operand,
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

	bool is_preg() const { return type() == operand_type::preg; }
	bool is_vreg() const { return type() == operand_type::vreg; }
	bool is_mem() const { return type() == operand_type::mem; }
	bool is_imm() const { return type() == operand_type::imm; }
    bool is_shift() const { return type() == operand_type::shift; }
    bool is_cond() const { return type() == operand_type::cond; }
    bool is_label() const { return type() == operand_type::label; }

    preg_operand &preg() { return std::get<preg_operand>(op_); }
    const preg_operand &preg() const { return std::get<preg_operand>(op_); }

    vreg_operand &vreg() { return std::get<vreg_operand>(op_); }
    const vreg_operand &vreg() const { return std::get<vreg_operand>(op_); }

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

    size_t width() const {
        switch (type()) {
        case operand_type::preg:
            return preg().width();
        case operand_type::vreg:
            return vreg().width();
        case operand_type::mem:
            return memory().base_width();
        case operand_type::imm:
            return immediate().width();
        default:
            return 0;
        }
    }

	void allocate(int index, ir::value_type value_type) {
		if (type() != operand_type::vreg)
			throw std::runtime_error("trying to allocate non-vreg");

		op_ = preg_operand(index, value_type);
	}

	void allocate_base(int index, ir::value_type value_type) {
		if (type() != operand_type::mem)
			throw std::runtime_error("trying to allocate non-mem");

        auto &memory = std::get<memory_operand>(op_);
		if (!memory.is_virtual())
			throw std::runtime_error("trying to allocate non-virtual membase ");

        memory.set_base_reg(preg_operand(index, value_type));
	}

	void dump(std::ostream &os) const;
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

	void dump(std::ostream &os) const;
    std::string dump() const;
	void kill() { opcode_.clear(); }

	bool is_dead() const { return opcode_.empty(); }

    std::string& opcode() { return opcode_; }
    const std::string& opcode() const { return opcode_; }

    size_t operand_count() const { return opcount_; }

	operand_array &operands() { return operands_; }
	const operand_array &operands() const { return operands_; }
private:
    std::string opcode_;

    size_t opcount_;
    operand_array operands_;
};

} // namespace arancini::output::dynamic::arm64

