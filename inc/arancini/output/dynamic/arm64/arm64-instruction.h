#pragma once

#include <keystone/keystone.h>

#include <arancini/output/dynamic/machine-code-writer.h>

#include <vector>
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

struct vreg_operand {
    size_t width;
	unsigned int index;

    vreg_operand() = default;

	vreg_operand(unsigned int i, size_t width)
		: width(width)
        , index(i)
	{
	}
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

    explicit preg_operand(regname32 reg): reg_(reg), width_(32) { }

    explicit preg_operand(regname64 reg): reg_(reg), width_(64) { }

    explicit preg_operand(size_t index, size_t width) {
        width_ = width;
        if (index > static_cast<size_t>(xzr_sp) + 1)
            throw std::runtime_error("Allocating unavailable register at index: "
                                     + std::to_string(index));
        if (width == 64)
            reg_ = static_cast<regname64>(index + 1);
        else if (width == 32)
            reg_ = static_cast<regname32>(index + 1);
        else if (width == 8)
            (void)0; // FIXME
        else
            throw std::runtime_error("Physical registers are specified as 32-bit or 64-bit only");
    }

    preg_operand(const preg_operand& r): reg_(r.reg_), width_(r.width_) { }

    preg_operand& operator=(const preg_operand& r) {
        reg_ = r.reg_;
        width_ = r.width_;
        return *this;
    }

    regname get() const { return reg_; }

    size_t width() const { return width_; }

    const char* to_string() const;
private:
    regname reg_;
    size_t width_ = 0;
};

struct memory_operand {
	bool virt_base;

    // TODO: add as union
    vreg_operand vbase;
    preg_operand pbase;

	int offset;
    bool pre_index = false;
    bool post_index = false;

    memory_operand() = default;

	explicit memory_operand(const preg_operand &base, int offset = 0,
                                  bool pre_index = false, bool post_index = false)
		: virt_base(false)
		, pbase(base)
		, offset(offset)
        , pre_index(pre_index)
        , post_index(post_index)
	{
        if (pre_index == post_index && pre_index)
            throw std::runtime_error("Both pre- and post-index passed to ARM DBT");
    }

	explicit memory_operand(const vreg_operand &virt_base, int offset = 0,
                         bool pre_index = false, bool post_index = false)
		: virt_base(true)
		, vbase(virt_base)
		, offset(offset)
        , pre_index(pre_index)
        , post_index(post_index)
    {
        if (pre_index == post_index && pre_index)
            throw std::runtime_error("Both pre- and post-index passed to ARM DBT");
    }

    memory_operand(const memory_operand& m)
        : virt_base(m.virt_base)
        , offset(m.offset)
        , pre_index(m.pre_index)
        , post_index(m.post_index)
    {
        if (m.virt_base) vbase = m.vbase;
        else pbase = m.pbase;
    }

    memory_operand& operator=(const memory_operand& m)
    {
        virt_base = m.virt_base;
        offset = m.offset;
        pre_index = m.pre_index;
        post_index = m.post_index;
        if (m.virt_base) vbase = m.vbase;
        else pbase = m.pbase;

        return *this;
    }

    size_t base_width() const {
        if (virt_base) return vbase.width;
        return pbase.width();
    }
};

// TODO: how are immediates represented in arm
// TODO: fix this
struct immediate_operand {
	union {
		unsigned long int u64;
		signed long int s64;
	};
    bool sign = 0;
    uint8_t width = 0;

    immediate_operand() = default;

	immediate_operand(unsigned long int v, uint8_t width)
		: u64(v)
        , sign(false)
        , width(width)
	{
        if (width && !fits(v, width))
            throw std::runtime_error("Specified immediate does not fit in width: " +
                                     std::to_string(width) + " " + std::to_string(v));
	}

	immediate_operand(signed long int v, uint8_t width)
		: s64(v)
        , sign(true)
        , width(width)
	{
        if (width && !fits(v, width))
            throw std::runtime_error("Specified immediate does not fit in width: " +
                                     std::to_string(width) + " " + std::to_string(v));
	}

    static bool fits(uint64_t v, uint8_t width) {
        return width == 64 || (v & ((1llu << width) - 1)) == v;
    }
};

struct shift_operand : public immediate_operand {
    // TODO: check fit in 48-bit shift specifier
    std::string modifier;

    shift_operand() = default;

    shift_operand(const std::string &modifier,
                        size_t amount = 0, size_t width = 64)
        : immediate_operand(amount, width)
    {
        this->modifier = modifier;
    }
};

struct label_operand {
    label_operand() = default;

    label_operand(const std::string &label): name(label)
    {
    }

    std::string name;
};

struct cond_operand {
    cond_operand() = default;

    cond_operand(const std::string &cond): cond(cond)
    {
    }

    std::string cond;
};

enum class operand_type { invalid, preg, vreg, mem, imm, shift, label, cond};

struct operand {
	operand_type type;
	union {
		preg_operand preg;
		vreg_operand vreg;
		memory_operand memory;
		immediate_operand immediate;
	};

    shift_operand shift;
    label_operand label;
    cond_operand condition;

	bool use, def;

	operand()
		: type(operand_type::invalid)
		, use(false)
		, def(false)
	{
	}

	operand(const preg_operand &o)
		: type(operand_type::preg)
		, preg(o)
		, use(false)
		, def(false)
	{
	}

	operand(const vreg_operand &o)
		: type(operand_type::vreg)
		, vreg(o)
		, use(false)
		, def(false)
	{
	}

	operand(const memory_operand &o)
		: type(operand_type::mem)
		, memory(o)
		, use(false)
		, def(false)
	{
	}

	operand(const immediate_operand &o)
		: type(operand_type::imm)
		, immediate(o)
		, use(false)
		, def(false)
	{
	}

	operand(const shift_operand &o)
		: type(operand_type::shift)
		, shift(o)
		, use(false)
		, def(false)
	{
	}

	operand(const label_operand &o)
		: type(operand_type::label)
		, label(o)
		, use(false)
		, def(false)
	{
	}

	operand(const cond_operand &o)
		: type(operand_type::cond)
		, condition(o)
		, use(false)
		, def(false)
	{
        // TODO: check cond
	}

    operand(const operand &o)
        : type(o.type),
          use(o.use),
          def(o.def)
    {
        if (type == operand_type::preg)
            preg = o.preg;
        if (type == operand_type::vreg)
            vreg = o.vreg;
        if (type == operand_type::mem)
            memory = o.memory;
        if (type == operand_type::imm)
            immediate = o.immediate;
        if (type == operand_type::shift)
            shift = o.shift;
        if (type == operand_type::cond)
            condition = o.condition;
        if (type == operand_type::label)
            label = o.label;
    }

    operand& operator=(const operand &o) {
        type = o.type;
        use = o.use;
        def = o.def;

        if (type == operand_type::preg)
            preg = o.preg;
        if (type == operand_type::vreg)
            vreg = o.vreg;
        if (type == operand_type::mem)
            memory = o.memory;
        if (type == operand_type::imm)
            immediate = o.immediate;
        if (type == operand_type::shift)
            shift = o.shift;
        if (type == operand_type::cond)
            condition = o.condition;
        if (type == operand_type::label)
            label = o.label;

        return *this;
    }

	bool is_preg() const { return type == operand_type::preg; }
	bool is_vreg() const { return type == operand_type::vreg; }
	bool is_mem() const { return type == operand_type::mem; }
	bool is_imm() const { return type == operand_type::imm; }
    bool is_shift() const { return type == operand_type::shift; }
    bool is_cond() const { return type == operand_type::cond; }
    bool is_label() const { return type == operand_type::label; }

	bool is_use() const { return use; }
	bool is_def() const { return def; }
	bool is_usedef() const { return use && def; }

    size_t width() const {
        switch (type) {
        case operand_type::preg:
            return preg.width();
        case operand_type::vreg:
            return vreg.width;
        case operand_type::mem:
            return memory.base_width();
        case operand_type::imm:
            return immediate.width;
        default:
            return 0;
        }
    }

	void allocate(int index, size_t width) {
		if (type != operand_type::vreg)
			throw std::runtime_error("trying to allocate non-vreg");

		type = operand_type::preg;

        // TODO: change
		preg = preg_operand(index, width);
	}

	void allocate_base(int index, size_t width) {
		if (type != operand_type::mem)
			throw std::runtime_error("trying to allocate non-mem");

		if (!memory.virt_base)
			throw std::runtime_error("trying to allocate non-virtual membase ");

		memory.virt_base = false;

        // TODO: change
		memory.pbase = preg_operand(index, width);
	}

	void dump(std::ostream &os) const;
};

static operand def(const operand &o)
{
    operand r = o;
    r.def = true;
    return r;
}

static operand use(const operand &o)
{
    operand r = o;
    r.use = true;
    return r;
}

static operand usedef(const operand &o)
{
    operand r = o;
    r.use = true;
    r.def = true;
    return r;
}

struct instruction {
	static constexpr size_t nr_operands = 5;

    std::string opcode;
    size_t opcount;

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

    static instruction add(const operand &dst,
                                 const operand &src1,
                                 const operand &src2) {
        return instruction("add", def(dst), use(src1), use(src2));
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

