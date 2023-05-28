#pragma once

#include <keystone/keystone.h>

#include <arancini/output/dynamic/machine-code-writer.h>

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

class vreg_operand {
public:
    vreg_operand() = default;

	vreg_operand(unsigned int i, size_t width)
		: width_(width)
        , index_(i)
	{
	}

    vreg_operand(const vreg_operand &o)
        : width_(o.width_)
        , index_(o.index_)
    {
    }

    vreg_operand &operator=(const vreg_operand &o) {
        width_ = o.width_;
        index_ = o.index_;
    }

    size_t width() const { return width_; }

    unsigned int index() const { return index_; }
private:
    size_t width_;
	unsigned int index_;
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

    using regname = uint8_t;

    preg_operand() = default;

    explicit preg_operand(regname32 reg): width_(32), reg_(reg) { }

    explicit preg_operand(regname64 reg): width_(64), reg_(reg) { }

    explicit preg_operand(size_t index, size_t width) {
        width_ = width;
        if (index > static_cast<size_t>(xzr_sp) + 1)
            throw std::runtime_error("Allocating unavailable register at index: "
                                     + std::to_string(index));
        if (width_ == 64)
            reg_ = static_cast<regname64>(index + 1);
        else if (width_ == 32)
            reg_ = static_cast<regname32>(index + 1);
        else if (width_ == 8)
            (void)0; // FIXME
        else
            throw std::runtime_error("Physical registers are specified as 32-bit or 64-bit only");
    }

    preg_operand(const preg_operand& r)
        : width_(r.width_)
        , reg_(r.reg_)
    { }

    preg_operand& operator=(const preg_operand& r) {
        reg_ = r.reg_;
        width_ = r.width_;
        return *this;
    }

    size_t width() const { return width_; }

    regname get() const { return reg_; }

    const char* to_string() const;
private:
    size_t width_;
    regname reg_;
};

class memory_operand {
public:
    memory_operand() = default;

    template<typename T>
	memory_operand(const T &base,
                   int offset = 0,
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
        return 0;
    }

    bool offset() const { return offset_; }
    bool pre_index() const { return pre_index_; }
    bool post_index() const { return post_index_; }
private:
    std::variant<vreg_operand, preg_operand> reg_base_;

	int offset_ = 0;
    bool pre_index_ = false;
    bool post_index_ = false;

};

// TODO: how are immediates represented in arm
// TODO: fix this
class immediate_operand {
public:
    immediate_operand() = default;

    template <typename T, typename std::enable_if<std::is_unsigned_v<T>>::type* = nullptr>
	immediate_operand(T v, uint8_t width)
		: u64_(v)
        , sign_(false)
        , width_(width)
	{
        if (width_ && !fits(v, width_))
            throw std::runtime_error("Specified immediate does not fit in width: " +
                                     std::to_string(width_) + " " + std::to_string(v));
	}

    template <typename T, typename std::enable_if<std::is_signed_v<T>>::type* = nullptr>
	immediate_operand(T v, uint8_t width)
		: s64_(v)
        , sign_(true)
        , width_(width)
	{
        if (width_ && !fits(v, width_))
            throw std::runtime_error("Specified immediate does not fit in width: " +
                                     std::to_string(width_) + " " + std::to_string(v));
	}

    static bool fits(uint64_t v, uint8_t width) {
        return width == 64 || (v & ((1llu << width) - 1)) == v;
    }

    size_t width() const { return width_; }

    size_t u64() const { return u64_; }
    size_t s64() const { return s64_; }
private:
	union {
		unsigned long int u64_;
		signed long int s64_;
	};

    bool sign_ = 0;
    size_t width_;

};

class shift_operand : public immediate_operand {
public:
    // TODO: check fit in 48-bit shift specifier
    shift_operand() = default;

    shift_operand(const std::string &modifier,
                        size_t amount = 0, size_t width = 64)
        : immediate_operand(amount, width)
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
    {
    }

    operand(const operand &o)
         : op_(o.op_)
         , use_(o.use_)
         , def_(o.def_)
    {
    }

    operand& operator=(const operand &o) {
        op_ = o.op_;
        use_ = o.use_;
        def_ = o.def_;

        return *this;
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
	bool is_usedef() const { return use_ && def_; }

    void set_use() { use_ = true; }
    void set_def() { def_ = true; }
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

	void allocate(int index, size_t width) {
		if (type() != operand_type::vreg)
			throw std::runtime_error("trying to allocate non-vreg");

		op_ = preg_operand(index, width);
	}

	void allocate_base(int index, size_t width) {
		if (type() != operand_type::mem)
			throw std::runtime_error("trying to allocate non-mem");

        auto &memory = std::get<memory_operand>(op_);
		if (!memory.is_virtual())
			throw std::runtime_error("trying to allocate non-virtual membase ");

        // TODO: change
		memory = preg_operand(index, width);
	}

	void dump(std::ostream &os) const;
protected:
    operand_variant op_;
	bool use_, def_;
};

static preg_operand preg_or_membase(const operand &o) {
    if (o.is_mem())
        return o.memory().preg_base();
    else
        return o.preg();
}

static operand def(const operand &o)
{
    operand r = o;
    r.set_def();
    return r;
}

static operand use(const operand &o)
{
    operand r = o;
    r.set_use();
    return r;
}

static operand usedef(const operand &o)
{
    operand r = o;
    r.set_usedef();
    return r;
}

struct instruction {
	static constexpr size_t nr_operands = 5;

    size_t opcount;
    std::string opcode;

    operand operands[nr_operands];

    instruction(const std::string& opc)
        : opcode(opc)
        , opcount(0)
	{
	}

	instruction(const std::string &opc, const operand &o1)
		: opcode(opc)
	{
		operands[0] = o1;
        opcount = 1;
	}

	instruction(const std::string &opc, const operand &o1, const operand &o2)
		: opcode(opc)
	{
		operands[0] = o1;
		operands[1] = o2;
        opcount = 2;
	}

	instruction(const std::string &opc,
                      const operand &o1,
                      const operand &o2,
                      const operand &o3)
		: opcode(opc)
	{
		operands[0] = o1;
		operands[1] = o2;
		operands[2] = o3;
        opcount = 3;
	}

	instruction(const std::string &opc,
                      const operand &o1,
                      const operand &o2,
                      const operand &o3,
                      const operand &o4)
		: opcode(opc)
	{
		operands[0] = o1;
		operands[1] = o2;
		operands[2] = o3;
		operands[3] = o4;
        opcount = 4;
	}

	void dump(std::ostream &os) const;
	void kill() { opcode.clear(); }

	bool is_dead() const { return opcode.empty(); }

	operand &get_operand(int index) { return operands[index]; }
	const operand &get_operand(int index) const { return operands[index]; }

	decltype(operands) &get_operands() { return operands; }
	const decltype(operands) &get_operands() const { return operands; }
};

} // namespace arancini::output::dynamic::arm64

