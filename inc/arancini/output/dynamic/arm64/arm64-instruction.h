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

struct arm64_vreg_op {
    size_t width;
	unsigned int index;

    arm64_vreg_op() = default;

	arm64_vreg_op(unsigned int i, size_t width)
		: width(width)
        , index(i)
	{
	}
};

class arm64_physreg_op {
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

    arm64_physreg_op() = default;

    explicit arm64_physreg_op(regname32 reg): reg_(reg), width_(32) { }

    explicit arm64_physreg_op(regname64 reg): reg_(reg), width_(64) { }

    explicit arm64_physreg_op(size_t index, size_t width) {
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

    arm64_physreg_op(const arm64_physreg_op& r): reg_(r.reg_), width_(r.width_) { }

    arm64_physreg_op& operator=(const arm64_physreg_op& r) {
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

struct arm64_memory_operand {
	bool virt_base;

    // TODO: add as union
    arm64_vreg_op vbase;
    arm64_physreg_op pbase;

	int offset;
    bool pre_index = false;
    bool post_index = false;

    arm64_memory_operand() = default;

	explicit arm64_memory_operand(const arm64_physreg_op &base, int offset = 0,
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

	explicit arm64_memory_operand(const arm64_vreg_op &virt_base, int offset = 0,
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

    arm64_memory_operand(const arm64_memory_operand& m)
        : virt_base(m.virt_base)
        , offset(m.offset)
        , pre_index(m.pre_index)
        , post_index(m.post_index)
    {
        if (m.virt_base) vbase = m.vbase;
        else pbase = m.pbase;
    }

    arm64_memory_operand& operator=(const arm64_memory_operand& m)
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
struct arm64_immediate_operand {
	union {
		unsigned char u8;
		unsigned short u16;
		unsigned int u32;
		unsigned long int u64;
		signed char s8;
		signed short s16;
		signed int s32;
		signed long int s64;
	};
    uint8_t width = 0;

    arm64_immediate_operand() = default;

	arm64_immediate_operand(unsigned long v, uint8_t width)
		: u64(v)
        , width(width)
	{
	}
};

struct arm64_shift_operand : public arm64_immediate_operand {
    // TODO: check fit in 48-bit shift specifier
    std::string modifier;

    arm64_shift_operand() = default;

    arm64_shift_operand(const std::string &modifier,
                        size_t amount = 0, size_t width = 64)
        : arm64_immediate_operand(amount, width)
    {
        this->modifier = modifier;
    }
};

struct arm64_label_operand {
    arm64_label_operand() = default;

    arm64_label_operand(const std::string &label): name(label)
    {
    }

    std::string name;
};

struct arm64_cond_operand {
    arm64_cond_operand() = default;

    arm64_cond_operand(const std::string &cond): cond(cond)
    {
    }

    std::string cond;
};

enum class arm64_operand_type { invalid, preg, vreg, mem, imm, shift, label, cond};

struct arm64_operand {
	arm64_operand_type type;
	union {
		arm64_physreg_op pregop;
		arm64_vreg_op vregop;
		arm64_memory_operand memop;
		arm64_immediate_operand immop;
	};

    arm64_cond_operand condop;
    arm64_label_operand labelop;
    arm64_shift_operand shiftop;

	bool use, def;

	arm64_operand()
		: type(arm64_operand_type::invalid)
		, use(false)
		, def(false)
	{
	}

	arm64_operand(const arm64_physreg_op &o)
		: type(arm64_operand_type::preg)
		, pregop(o)
		, use(false)
		, def(false)
	{
	}

	arm64_operand(const arm64_vreg_op &o)
		: type(arm64_operand_type::vreg)
		, vregop(o)
		, use(false)
		, def(false)
	{
	}

	arm64_operand(const arm64_memory_operand &o)
		: type(arm64_operand_type::mem)
		, memop(o)
		, use(false)
		, def(false)
	{
	}

	arm64_operand(const arm64_immediate_operand &o)
		: type(arm64_operand_type::imm)
		, immop(o)
		, use(false)
		, def(false)
	{
	}

	arm64_operand(const arm64_shift_operand &o)
		: type(arm64_operand_type::shift)
		, shiftop(o)
		, use(false)
		, def(false)
	{
	}

	arm64_operand(const arm64_label_operand &o)
		: type(arm64_operand_type::label)
		, labelop(o)
		, use(false)
		, def(false)
	{
	}

	arm64_operand(const arm64_cond_operand &o)
		: type(arm64_operand_type::cond)
		, condop(o)
		, use(false)
		, def(false)
	{
        // TODO: check cond
	}

    arm64_operand(const arm64_operand &o)
        : type(o.type),
          use(o.use),
          def(o.def)
    {
        if (type == arm64_operand_type::preg)
            pregop = o.pregop;
        if (type == arm64_operand_type::vreg)
            vregop = o.vregop;
        if (type == arm64_operand_type::mem)
            memop = o.memop;
        if (type == arm64_operand_type::imm)
            immop = o.immop;
        if (type == arm64_operand_type::shift)
            shiftop = o.shiftop;
        if (type == arm64_operand_type::cond)
            condop = o.condop;
    }

    arm64_operand& operator=(const arm64_operand &o) {
        type = o.type;
        use = o.use;
        def = o.def;

        if (type == arm64_operand_type::preg)
            pregop = o.pregop;
        if (type == arm64_operand_type::vreg)
            vregop = o.vregop;
        if (type == arm64_operand_type::mem)
            memop = o.memop;
        if (type == arm64_operand_type::imm)
            immop = o.immop;
        if (type == arm64_operand_type::shift)
            shiftop = o.shiftop;
        if (type == arm64_operand_type::cond)
            condop = o.condop;

        return *this;
    }

	bool is_preg() const { return type == arm64_operand_type::preg; }
	bool is_vreg() const { return type == arm64_operand_type::vreg; }
	bool is_mem() const { return type == arm64_operand_type::mem; }
	bool is_imm() const { return type == arm64_operand_type::imm; }
    bool is_shift() const { return type == arm64_operand_type::shift; }
    bool is_cond() const { return type == arm64_operand_type::cond; }

	bool is_use() const { return use; }
	bool is_def() const { return def; }
	bool is_usedef() const { return use && def; }

    size_t width() const {
        switch (type) {
        case arm64_operand_type::preg:
            return pregop.width();
        case arm64_operand_type::vreg:
            return vregop.width;
        case arm64_operand_type::mem:
            return memop.base_width();
        case arm64_operand_type::imm:
            return immop.width;
        default:
            return 0;
        }
    }

	void allocate(int index, size_t width) {
		if (type != arm64_operand_type::vreg)
			throw std::runtime_error("trying to allocate non-vreg");

		type = arm64_operand_type::preg;

        // TODO: change
		pregop = arm64_physreg_op(index, width);
	}

	void allocate_base(int index, size_t width) {
		if (type != arm64_operand_type::mem)
			throw std::runtime_error("trying to allocate non-mem");

		if (!memop.virt_base)
			throw std::runtime_error("trying to allocate non-virtual membase ");

		memop.virt_base = false;

        // TODO: change
		memop.pbase = arm64_physreg_op(index, width);
	}

	void dump(std::ostream &os) const;
};

#define OPFORM_BITS(T, W) (((T & 3) << 4) | ((W / 8) & 15))
#define GET_OPFORM1(T1, W1) OPFORM_BITS(T1, W1)
#define GET_OPFORM2(T1, W1, T2, W2) ((OPFORM_BITS(T2, W2) << 6) | OPFORM_BITS(T1, W1))
#define GET_OPFORM3(T1, W1, T2, W2, T3, W3) ((OPFORM_BITS(T3, W3) << 12) | (OPFORM_BITS(T2, W2) << 6) | OPFORM_BITS(T1, W1))

#define R 1
#define M 2
#define I 3
#define DEFINE_OPFORM1(T1, W1) OF_##T1##W1 = GET_OPFORM1(T1, W1)
#define DEFINE_OPFORM2(T1, W1, T2, W2) OF_##T1##W1##_##T2##W2 = GET_OPFORM2(T1, W1, T2, W2)
#define DEFINE_OPFORM3(T1, W1, T2, W2, T3, W3) OF_##T1##W1##_##T2##W2##_##T3##W3 = GET_OPFORM3(T1, W1, T2, W2, T3, W3)

enum class arm64_opform {
	OF_NONE = 0,
	DEFINE_OPFORM1(R, 32),
	DEFINE_OPFORM1(M, 32),
	DEFINE_OPFORM1(I, 32),
	DEFINE_OPFORM1(R, 64),
	DEFINE_OPFORM1(M, 64),
	DEFINE_OPFORM1(I, 64),
	DEFINE_OPFORM2(R, 32, R, 32),
	DEFINE_OPFORM2(R, 64, R, 64),
	DEFINE_OPFORM2(R, 32, M, 32),
	DEFINE_OPFORM2(R, 64, M, 64),
	DEFINE_OPFORM2(M, 32, R, 32),
	DEFINE_OPFORM2(M, 64, R, 64),
	DEFINE_OPFORM2(R, 32, I, 32),
	DEFINE_OPFORM2(R, 64, I, 64),

	DEFINE_OPFORM3(R, 64, R, 64, I, 64)
};

#undef R
#undef M
#undef I

struct arm64_instruction {
	static constexpr size_t nr_operands = 5;

    std::string opcode;
    arm64_opform opform;
    size_t opcount;

    arm64_operand operands[nr_operands];

    arm64_instruction(const std::string& opc)
        : opcode(opc)
		, opform(arm64_opform::OF_NONE)
        , opcount(0)
	{
	}

	arm64_instruction(const std::string &opc, const arm64_operand &o1)
		: opcode(opc)
		, opform(classify(o1))
	{
		operands[0] = o1;
        opcount = 1;
	}

	arm64_instruction(const std::string &opc, const arm64_operand &o1, const arm64_operand &o2)
		: opcode(opc)
		, opform(classify(o1, o2))
	{
		operands[0] = o1;
		operands[1] = o2;
        opcount = 2;
	}

	arm64_instruction(const std::string &opc,
                      const arm64_operand &o1,
                      const arm64_operand &o2,
                      const arm64_operand &o3)
		: opcode(opc)
		, opform(classify(o1, o2, o3))
	{
		operands[0] = o1;
		operands[1] = o2;
		operands[2] = o3;
        opcount = 3;
	}

	arm64_instruction(const std::string &opc,
                      const arm64_operand &o1,
                      const arm64_operand &o2,
                      const arm64_operand &o3,
                      const arm64_operand &o4)
		: opcode(opc)
		, opform(classify(o1, o2, o3))
	{
		operands[0] = o1;
		operands[1] = o2;
		operands[2] = o3;
		operands[3] = o4;
        opcount = 4;
	}

	static int operand_type_to_form_type(arm64_operand_type t)
	{
		switch (t) {
		case arm64_operand_type::preg:
		case arm64_operand_type::vreg:
			return 1;
		case arm64_operand_type::mem:
			return 2;
		case arm64_operand_type::imm:
			return 3;

		default:
			return 0;
		}
	}

	static arm64_opform classify(const arm64_operand &o1) {
        return (arm64_opform)GET_OPFORM1(operand_type_to_form_type(o1.type), o1.width());
    }

	static arm64_opform classify(const arm64_operand &o1, const arm64_operand &o2) {
		return (arm64_opform)GET_OPFORM2(operand_type_to_form_type(o1.type), o1.width(),
                                         operand_type_to_form_type(o2.type), o2.width());
	}

	static arm64_opform classify(const arm64_operand &o1, const arm64_operand &o2, const arm64_operand &o3)
	{
		return (arm64_opform)GET_OPFORM3(operand_type_to_form_type(o1.type), o1.width(),
                                         operand_type_to_form_type(o2.type), o2.width(),
                                         operand_type_to_form_type(o3.type), o3.width());
	}

    // TODO: handle this
	static std::string opform_to_string(arm64_opform of)
	{
		std::stringstream ss;

		unsigned int raw_of = (unsigned int)of;

		for (int i = 0; i < 4; i++) {
			int off = i * 6;
			int width = (raw_of >> off) & 0xf;
			int type = (raw_of >> (off + 4)) & 0x3;

			switch (type) {
			case 0:
				continue;

			case 1:
				ss << "R";
				break;
			case 2:
				ss << "M";
				break;
			case 3:
				ss << "I";
				break;
			}

			ss << std::dec << (width * 8);
		}

		return ss.str();
	}

	static arm64_operand def(const arm64_operand &o)
	{
		arm64_operand r = o;
		r.def = true;
		return r;
	}

	static arm64_operand use(const arm64_operand &o)
	{
		arm64_operand r = o;
		r.use = true;
		return r;
	}

	static arm64_operand usedef(const arm64_operand &o)
	{
		arm64_operand r = o;
		r.use = true;
		r.def = true;
		return r;
	}

    static arm64_instruction add(const arm64_operand &dst,
                                 const arm64_operand &src1,
                                 const arm64_operand &src2) {
        return arm64_instruction("add", def(dst), use(src1), use(src2));
    }

    static arm64_instruction add(const arm64_operand &dst,
                                 const arm64_operand &src1,
                                 const arm64_operand &src2,
                                 const arm64_operand &shift) {
        return arm64_instruction("add", def(dst), use(src1), use(src2), use(shift));
    }

    static arm64_instruction sub(const arm64_operand &dst,
                                 const arm64_operand &src1,
                                 const arm64_operand &src2) {
        return arm64_instruction("sub", def(dst), use(src1), use(src2));
    }

    static arm64_instruction sub(const arm64_operand &dst,
                                 const arm64_operand &src1,
                                 const arm64_operand &src2,
                                 const arm64_operand &shift) {
        return arm64_instruction("sub", def(dst), use(src1), use(src2), use(shift));
    }

    static arm64_instruction or_(const arm64_operand &dst,
                                 const arm64_operand &src1,
                                 const arm64_operand &src2) {
        return arm64_instruction("orr", def(dst), use(src1), use(src2));
    }

    static arm64_instruction and_(const arm64_operand &dst,
                                  const arm64_operand &src1,
                                  const arm64_operand &src2) {
        return arm64_instruction("and", def(dst), use(src1), use(src2));
    }

    static arm64_instruction xor_(const arm64_operand &dst,
                                  const arm64_operand &src1,
                                  const arm64_operand &src2) {
        return arm64_instruction("xor", def(dst), use(src1), use(src2));
    }

    static arm64_instruction not_(const arm64_operand &dst, const arm64_operand &src) {
        return arm64_instruction("mvn", def(dst), use(src));
    }

    static arm64_instruction moveq(const arm64_operand &dst, const arm64_immediate_operand &src) {
        return arm64_instruction("moveq", def(dst), use(src));
    }

    static arm64_instruction movss(const arm64_operand &dst, const arm64_immediate_operand &src) {
        return arm64_instruction("movss", def(dst), use(src));
    }

    // TODO: express dependency on flag reg
    static arm64_instruction movvs(const arm64_operand &dst, const arm64_immediate_operand &src) {
        return arm64_instruction("movvs", def(dst), use(src));
    }

    static arm64_instruction movcs(const arm64_operand &dst, const arm64_immediate_operand &src) {
        return arm64_instruction("movcs", def(dst), use(src));
    }

    static arm64_instruction movn(const arm64_operand &dst,
                                  const arm64_operand &src,
                                  const arm64_operand &shift) {
        return arm64_instruction("movn", def(dst), use(src), use(shift));
    }

    static arm64_instruction movz(const arm64_operand &dst,
                                  const arm64_operand &src,
                                  const arm64_operand &shift) {
        return arm64_instruction("movz", def(dst), use(src), use(shift));
    }

    static arm64_instruction mov(const arm64_operand &dst, const arm64_operand &src) {
        return arm64_instruction("mov", def(dst), use(src));
    }

    static arm64_instruction b(const std::string &src) {
        return arm64_instruction("b", use(arm64_label_operand(src)));
    }

    static arm64_instruction beq(const std::string &src) {
        return arm64_instruction("beq", use(arm64_label_operand(src)));
    }

    static arm64_instruction bl(const std::string &name) {
        return arm64_instruction("bl", use(arm64_label_operand(name)));
    }

    static arm64_instruction cmp(const arm64_operand &dst,
                                 const arm64_operand &src1,
                                 const arm64_operand &src2) {
        return arm64_instruction("cmp", def(dst), use(src1), use(src2));
    }

    static arm64_instruction lsl(const arm64_operand &dst,
                                 const arm64_operand &src1,
                                 const arm64_operand &src2) {
        return arm64_instruction("lsl", def(dst), use(src1), use(src2));
    }

    static arm64_instruction lsr(const arm64_operand &dst,
                                 const arm64_operand &src1,
                                 const arm64_operand &src2) {
        return arm64_instruction("lsr", def(dst), use(src1), use(src2));
    }

    static arm64_instruction asr(const arm64_operand &dst,
                                 const arm64_operand &src1,
                                 const arm64_operand &src2) {
        return arm64_instruction("asr", def(dst), use(src1), use(src2));
    }

    static arm64_instruction csel(const arm64_operand &dst,
                                  const arm64_operand &src1,
                                  const arm64_operand &src2,
                                  const arm64_operand &cond) {
        return arm64_instruction("csel", def(dst), use(src1), use(src2), use(cond));
    }

    static arm64_instruction cset(const arm64_operand &dst,
                                  const arm64_operand &cond) {
        return arm64_instruction("cset", def(dst), use(cond));
    }

    static arm64_instruction ubfx(const arm64_operand &dst,
                                  const arm64_operand &src1,
                                  const arm64_operand &src2,
                                  const arm64_operand &cond) {
        return arm64_instruction("ubfx", def(dst), use(src1), use(src2), use(cond));
    }

    static arm64_instruction bfi(const arm64_operand &dst,
                                 const arm64_operand &src1,
                                 const arm64_operand &src2,
                                 const arm64_operand &cond) {
        return arm64_instruction("bfi", def(dst), use(src1), use(src2), use(cond));
    }

    static arm64_instruction ldr(const arm64_operand &dst,
                                 const arm64_operand &base) {
        return arm64_instruction("ldr", def(dst), use(base));
    }

    static arm64_instruction str(const arm64_operand &src,
                                 const arm64_operand &base) {
        return arm64_instruction("str", use(src), def(base));
    }

    static arm64_instruction mul(const arm64_operand &dest,
                                 const arm64_operand &src1,
                                 const arm64_operand &src2) {
        return arm64_instruction("mul", def(dest), use(src1), use(src2));
    }

    static arm64_instruction sdiv(const arm64_operand &dest,
                                  const arm64_operand &src1,
                                  const arm64_operand &src2) {
        return arm64_instruction("sdiv", def(dest), use(src1), use(src2));
    }

    static arm64_instruction cmp(const arm64_operand &src1,
                                 const arm64_operand &src2) {
        return arm64_instruction("cmp", use(src1), use(src2));
    }

    static arm64_instruction ret() {
        return arm64_instruction("ret");
    }

    static arm64_instruction label(const std::string &label) {
        return arm64_instruction(label + ":");
    }

	void dump(std::ostream &os) const;
	void kill() { opcode.clear(); }

	bool is_dead() const { return opcode.empty(); }

	arm64_operand &get_operand(int index) { return operands[index]; }
	const arm64_operand &get_operand(int index) const { return operands[index]; }

	decltype(operands) &get_operands() { return operands; }
	const decltype(operands) &get_operands() const { return operands; }
};

} // namespace arancini::output::dynamic::arm64

