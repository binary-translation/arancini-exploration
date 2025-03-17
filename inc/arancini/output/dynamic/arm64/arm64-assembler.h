#pragma once

#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <keystone/keystone.h>

namespace arancini::output::dynamic::arm64 {

class arm64_assembler {
public:
    arm64_assembler() {
        status_ = ks_open(KS_ARCH_ARM64, KS_MODE_LITTLE_ENDIAN, &ks_);

        if (status_ != KS_ERR_OK)
            throw backend_exception("failed to initialise keystone assembler: {}", ks_strerror(status_));
    }

	struct add : instruction {
		add(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2): 
			instruction("add", def(dst), use(src1), use(src2))
		{ }

		add(const register_operand &dst, const register_operand &src1, 
	  		const reg_or_imm &src2, const shift_operand &shift):
			instruction("add", def(dst), use(src1), use(src2), use(shift))
		{ }
    };

	struct adds : instruction {
		adds(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2): 
			instruction("adds", def(dst), use(src1), use(src2))
		{
            implicitly_writes({register_operand(register_operand::nzcv)});
		}

		adds(const register_operand &dst, const register_operand &src1, 
	  		 const reg_or_imm &src2, const shift_operand &shift):
			instruction("adds", def(dst), use(src1), use(src2), use(shift))
		{ 
            implicitly_writes({register_operand(register_operand::nzcv)});
		}
    };

	struct adc : instruction {
		adc(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2):
			instruction("adc", def(dst), use(src1), use(src2))
		{
			  implicitly_reads({register_operand(register_operand::nzcv)});
		}
    };

	struct adcs : instruction {
		adcs(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2):
			instruction("adcs", def(dst), use(src1), use(src2))
		{
			  implicitly_reads({register_operand(register_operand::nzcv)});
			  implicitly_writes({register_operand(register_operand::nzcv)});
		}
    };

	struct sub : instruction {
		sub(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2): 
			instruction("sub ", def(dst), use(src1), use(src2))
		{ }

		sub(const register_operand &dst, const register_operand &src1, 
	  		const reg_or_imm &src2, const shift_operand &shift):
			instruction("sub ", def(dst), use(src1), use(src2), use(shift))
		{ }
    };

	struct sbc : instruction {
		sbc(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2):
			instruction("sbc", def(dst), use(src1), use(src2))
		{
			implicitly_reads({register_operand(register_operand::nzcv)});
		}
	};

	struct subs : instruction {
		subs(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2): 
			instruction("subs", def(dst), use(src1), use(src2))
		{
            implicitly_writes({register_operand(register_operand::nzcv)});
		}

		subs(const register_operand &dst, const register_operand &src1, 
	  		 const reg_or_imm &src2, const shift_operand &shift):
			instruction("subs", def(dst), use(src1), use(src2), use(shift))
		{ 
            implicitly_writes({register_operand(register_operand::nzcv)});
		}
    };

	struct sbcs : instruction {
		sbcs(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2):
			instruction("sbcs", def(dst), use(src1), use(src2))
		{
			  implicitly_reads({register_operand(register_operand::nzcv)});
			  implicitly_writes({register_operand(register_operand::nzcv)});
		}
    };

    struct and_ : instruction {
		and_(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2):
			instruction("and", def(dst), use(src1), use(check_immediate(src2, ir::value_type::u(12)))) 
		{ }
    };

    struct ands : instruction {
		ands(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2):
			instruction("ands", def(dst), use(src1), use(check_immediate(src2, ir::value_type::u(12)))) 
		{
        	implicitly_writes({register_operand(register_operand::nzcv)});
		}
    };

    struct orr : instruction {
		orr(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2):
			instruction("orr", def(dst), use(src1), use(check_immediate(src2, ir::value_type::u(12)))) 
		{ }
    };

    struct eor : instruction {
		eor(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2):
			instruction("eor", def(dst), use(src1), use(check_immediate(src2, ir::value_type::u(12)))) 
		{ }
    };

    struct mvn : instruction {
		mvn(const register_operand &dst, const register_operand &src1):
			instruction("mvn", def(dst), use(src1))
		{ }
    };

    struct neg : instruction {
		neg(const register_operand &dst, const register_operand &src1):
			instruction("neg", def(dst), use(src1))
		{ }
    };

	struct mov : instruction {
		mov(const register_operand &dst, const reg_or_imm &src):
			instruction("mov", def(dst), use(check_immediate(src, ir::value_type::u(12))))
		{ }
	};

	struct movn : instruction {
		movn(const register_operand &dst, const immediate_operand &src, const shift_operand &shift):
			instruction("movn", def(dst), use(src), use(shift))
		{ }
	};

	struct movz : instruction {
		movz(const register_operand &dst, const immediate_operand &src, const shift_operand &shift):
			instruction("movz", def(dst), use(check_immediate(src, ir::value_type::u(16))), use(shift))
		{ }
	};

	struct movk : instruction {
		movk(const register_operand &dst, const immediate_operand &src, const shift_operand &shift):
			instruction("movk", use(def(dst)), use(check_immediate(src, ir::value_type::u(16))), use(shift))
		{ }
	};

	struct cmp : instruction {
		cmp(const register_operand &dst, const reg_or_imm &src):
			instruction("cmp", use(dst), use(check_immediate(src, ir::value_type::u(12))))
		{
			implicitly_writes({register_operand(register_operand::nzcv)});
		}
	};

	struct tst : instruction {
		tst(const register_operand &dst, const reg_or_imm &src):
			instruction("tst", use(dst), use(check_immediate(src, ir::value_type::u(12))))
		{
			implicitly_writes({register_operand(register_operand::nzcv)});
		}

		tst(const register_operand &dst, const reg_or_imm &src, const shift_operand& shift):
			instruction("tst", use(dst), use(check_immediate(src, ir::value_type::u(12))), use(shift))
		{
			implicitly_writes({register_operand(register_operand::nzcv)});
		}
	};

	struct cmn : instruction {
		cmn(const register_operand &dst, const reg_or_imm &src):
			instruction("cmn", use(dst), use(check_immediate(src, ir::value_type::u(12))))
		{
			implicitly_writes({register_operand(register_operand::nzcv)});
		}
	};

	struct b : instruction {
		b(label_operand &dest) : instruction("b", use(dest)) {
			as_branch();
		}
	};

    struct beq : instruction {
		beq(label_operand &dest): instruction("beq", use(dest)) {
			as_branch();
			implicitly_reads({register_operand(register_operand::nzcv)});
		}
	};

    struct bl : instruction {
		bl(label_operand &dest): instruction("bl", use(dest)) {
			as_branch();
			implicitly_reads({register_operand(register_operand::nzcv)});
		}
	};

    struct bne : instruction {
		bne(label_operand &dest): instruction("bne", use(dest)) {
			as_branch();
			implicitly_reads({register_operand(register_operand::nzcv)});
		}
	};

    // Check reg == 0 and jump if true
    // Otherwise, continue to the next instruction
    // Does not affect condition flags (can be used to compare-and-branch with 1 instruction)
    struct cbz : instruction {
		cbz(const register_operand& reg, const label_operand &dest): 
			instruction("cbz", use(reg), use(dest)) 
		{
			as_branch();
		}
	};

    // Check reg == 0 and jump if false
    // Otherwise, continue to the next instruction
    // Does not affect condition flags (can be used to compare-and-branch with 1 instruction)
    struct cbnz : instruction {
		cbnz(const register_operand& reg, const label_operand &dest): 
			instruction("cbnz", use(reg), use(dest)) 
		{
			as_branch();
		}
	};

	struct lsl : instruction {
		lsl(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2):
			instruction("lsl", def(dst), use(src1), use(src2))
		{
			std::size_t size = dst.type().element_width() <= 32 ? 5 : 6;
			check_immediate(src2, ir::value_type::u(size));
		}
	};

	struct lsr : instruction {
		lsr(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2):
			instruction("lsr", def(dst), use(src1), use(src2))
		{
			std::size_t size = dst.type().element_width() <= 32 ? 5 : 6;
			check_immediate(src2, ir::value_type::u(size));
		}
	};


	struct asr : instruction {
		asr(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2):
			instruction("asr", def(dst), use(src1), use(src2))
		{
			std::size_t size = dst.type().element_width() <= 32 ? 5 : 6;
			check_immediate(src2, ir::value_type::u(size));
		}
	};


	struct extr : instruction {
		extr(const register_operand &dst, const register_operand &src1, const register_operand &src2, const immediate_operand& shift):
			instruction("extr", def(dst), use(src1), use(src2), use(shift))
		{
			std::size_t size = dst.type().element_width() <= 32 ? 5 : 6;
			check_immediate(shift, ir::value_type::u(size));
		}
	};

	struct csel : instruction {
		csel(const register_operand &dst, const register_operand &src1, const register_operand &src2, const cond_operand& cond):
			instruction("csel", def(dst), use(src1), use(src2), use(cond))
		{ }
	};

	struct cset : instruction {
		cset(const register_operand &dst, const cond_operand& cond):
			instruction("cset", def(dst), use(cond))
		{ }
	};

	struct bfxil : instruction {
		bfxil(const register_operand& dst, const register_operand& src1,
			  const immediate_operand& lsb, const immediate_operand& width):
			instruction("bfxil", def(dst), use(src1), use(lsb), use(width))
		{
			auto element_size = dst.type().element_width() <= 32 ? 32 : 64;
			std::size_t bitsize = element_size == 32 ? 5 : 6;
			
			[[unlikely]]
			if (width.value() > element_size - lsb.value())
				throw backend_exception("Invalid width immediate {} for BFXIL instruction must fit into [1,{}] for lsb",
										width, element_size - lsb.value(), lsb);

			check_immediate(lsb, ir::value_type::u(bitsize));
		}
	};

	struct ubfx : instruction {
		ubfx(const register_operand& dst, const register_operand& src1,
			 const immediate_operand& lsb, const immediate_operand& width):
			instruction("ubfx", def(dst), use(src1), use(lsb), use(width))
		{
			auto element_size = dst.type().element_width() <= 32 ? 32 : 64;
			std::size_t bitsize = element_size == 32 ? 5 : 6;
			
			[[unlikely]]
			if (width.value() > element_size - lsb.value())
				throw backend_exception("Invalid width immediate {} for UBFX instruction must fit into [1,{}] for lsb",
										width, element_size - lsb.value(), lsb);

			check_immediate(lsb, ir::value_type::u(bitsize));
		}
	};

	struct bfi : instruction {
		bfi(const register_operand& dst, const register_operand& src1,
			const immediate_operand& lsb, const immediate_operand& width):
			instruction("bfi", use(def(dst)), use(src1), use(lsb), use(width))
		{
			auto element_size = dst.type().element_width() <= 32 ? 32 : 64;
			std::size_t bitsize = element_size == 32 ? 5 : 6;
			
			[[unlikely]]
			if (width.value() > element_size - lsb.value())
				throw backend_exception("Invalid width immediate {} for BFI instruction must fit into [1,{}] for lsb",
										width, element_size - lsb.value(), lsb);

			check_immediate(lsb, ir::value_type::u(bitsize));
		}
	};

	struct ldrb : instruction {
		ldrb(const register_operand& dest, const memory_operand& base):
			instruction("ldrb", def(dest), use(base))
		{ }
	};

	struct ldrh : instruction {
		ldrh(const register_operand& dest, const memory_operand& base):
			instruction("ldrh", def(dest), use(base))
		{ }
	};

	struct ldr : instruction {
		ldr(const register_operand& dest, const memory_operand& base):
			instruction("ldr", def(dest), use(base))
		{ }
	};

	struct strb : instruction {
		strb(const register_operand& dest, const memory_operand& base):
			instruction("strb", use(dest), use(base))
		{ }
	};

	struct strh : instruction {
		strh(const register_operand& dest, const memory_operand& base):
			instruction("strh", use(dest), use(base))
		{ }
	};

	struct str : instruction {
		str(const register_operand& dest, const memory_operand& base):
			instruction("str", use(dest), use(base))
		{ }
	};

	struct sxtb : instruction {
		sxtb(const register_operand& dst, const register_operand& src):
			instruction("sxtb", def(dst), use(src))
		{ }
	};

	struct sxth : instruction {
		sxth(const register_operand& dst, const register_operand& src):
			instruction("sxth", def(dst), use(src))
		{ }
	};

	struct sxtw : instruction {
		sxtw(const register_operand& dst, const register_operand& src):
			instruction("sxtw", def(dst), use(src))
		{ }
	};

	struct uxtb : instruction {
		uxtb(const register_operand& dst, const register_operand& src):
			instruction("uxtb", def(dst), use(src))
		{ }
	};

	struct uxth : instruction {
		uxth(const register_operand& dst, const register_operand& src):
			instruction("uxth", def(dst), use(src))
		{ }
	};

	struct uxtw : instruction {
		uxtw(const register_operand& dst, const register_operand& src):
			instruction("uxtw", def(dst), use(src))
		{ }
	};

	struct mul : instruction {
		mul(const register_operand& dest, const register_operand& src1, const register_operand& src2):
			instruction("mul", def(dest), use(src1), use(src2))
		{ }
	};

	struct smulh : instruction {
		smulh(const register_operand& dest, const register_operand& src1, const register_operand& src2):
			instruction("smulh", def(dest), use(src1), use(src2))
		{ }
	};

	struct smull : instruction {
		smull(const register_operand& dest, const register_operand& src1, const register_operand& src2):
			instruction("smull", def(dest), use(src1), use(src2))
		{ }
	};

	struct umulh : instruction {
		umulh(const register_operand& dest, const register_operand& src1, const register_operand& src2):
			instruction("umulh", def(dest), use(src1), use(src2))
		{ }
	};

	struct umull : instruction {
		umull(const register_operand& dest, const register_operand& src1, const register_operand& src2):
			instruction("umull", def(dest), use(src1), use(src2))
		{ }
	};

	struct sdiv : instruction {
		sdiv(const register_operand& dest, const register_operand& src1, const register_operand& src2):
			instruction("sdiv", def(dest), use(src1), use(src2))
		{ }
	};

	struct udiv : instruction {
		udiv(const register_operand& dest, const register_operand& src1, const register_operand& src2):
			instruction("udiv", def(dest), use(src1), use(src2))
		{ }
	};

	struct fmov : instruction {
		fmov(const register_operand& dest, const register_operand& src):
			instruction("fmov", def(dest), use(src))
		{
			[[unlikely]]
			if (dest.type().type_class() != ir::value_type_class::floating_point && 
				src.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("Either first or second operand of fmov {}, {} must be floating-point",
										dest.type(), src.type());
		}

		fmov(const register_operand& dest, const immediate_operand& src):
			instruction("fmov", def(dest), use(src))
		{ 
			[[unlikely]]
			if (dest.type().type_class() != ir::value_type_class::floating_point && 
				src.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("First operand of fmov {}, {} must be floating-point",
										dest.type(), src.type());
		}
	};

	struct fcmp : instruction { 
		fcmp(const register_operand& dest, const register_operand& src):
			instruction("fcmp", use(dest), use(src))
		{
			[[unlikely]]
			if (src.type().type_class() != ir::value_type_class::floating_point ||
				dest.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("The first and second operands of fcmp {}, {} must be floating-point registers",
										dest.type(), src.type());

			implicitly_writes({register_operand(register_operand::nzcv)});
		}
	};

	struct fmul : instruction {
		fmul(const register_operand& dest, const register_operand& src1, const register_operand& src2):
			instruction("fmul", def(dest), use(src1), use(src2))
		{ 
			if (src1.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("Second operand of fmul must be floating-point instead of {}",
										src1.type());
			if (src2.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("Third operand of fmul must be floating-point instead of {}",
										src1.type());
		}
	};

	struct fdiv : instruction {
		fdiv(const register_operand& dest, const register_operand& src1, const register_operand& src2):
			instruction("fdiv", def(dest), use(src1), use(src2))
		{
			[[unlikely]]
			if (src1.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("Second operand of fdiv must be floating-point instead of {}",
										src1.type());

			[[unlikely]]
			if (src2.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("Third operand of fdiv must be floating-point instead of {}",
										src1.type());
		}
	};

	struct fcvt : instruction {
		fcvt(const register_operand& dest, const register_operand& src):
			instruction("fcvt", def(dest), use(src))
		{
			[[unlikely]]
			if (src.type().type_class() != ir::value_type_class::floating_point &&
				dest.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("Either the first or second operand of fcvt {}, {} must be a floating-point register",
										dest.type(), src.type());
		}
	};

	struct fcvtzs : instruction {
		fcvtzs(const register_operand& dest, const register_operand& src):
			instruction("fcvtzs", def(dest), use(src))
		{
			[[unlikely]]
			if (src.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("Second operand of fcvt {}, {} must be a floating-point register",
										dest.type(), src.type());
			[[unlikely]]
			if (src.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("First operand of fcvt {}, {} must be a general-purpose register",
										dest.type(), src.type());
		}
	};

	struct fcvtzu : instruction {
		fcvtzu(const register_operand& dest, const register_operand& src):
			instruction("fcvtzu", def(dest), use(src))
		{
			[[unlikely]]
			if (src.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("Second operand of fcvtzu {}, {} must be a floating-point register",
										dest.type(), src.type());
			[[unlikely]]
			if (src.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("First operand of fcvtzu {}, {} must be a general-purpose register",
										dest.type(), src.type());
		}
	};

	struct fcvtas : instruction {
		fcvtas(const register_operand& dest, const register_operand& src):
			instruction("fcvtas", def(dest), use(src))
		{
			[[unlikely]]
			if (src.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("Second operand of fcvtas {}, {} must be a floating-point register",
										dest.type(), src.type());
			[[unlikely]]
			if (dest.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("First operand of fcvtas {}, {} must be a general-purpose register",
										dest.type(), src.type());
		}
	};

	struct fcvtau : instruction {
		fcvtau(const register_operand& dest, const register_operand& src):
			instruction("fcvtau", def(dest), use(src))
		{
			[[unlikely]]
			if (src.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("Second operand of fcvtau {}, {} must be a floating-point register",
										dest.type(), src.type());
			[[unlikely]]
			if (dest.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("First operand of fcvtau {}, {} must be a general-purpose register",
										dest.type(), src.type());
		}
	};

	struct scvtf : instruction {
		scvtf(const register_operand& dest, const register_operand& src):
			instruction("scvtf", def(dest), use(src))
		{ 
			[[unlikely]]
			if (src.type().type_class() == ir::value_type_class::floating_point)
				throw backend_exception("Second operand of scvtf {}, {} must be a general-purpose register",
										dest.type(), src.type());
			[[unlikely]]
			if (dest.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("First operand of scvtf {}, {} must be a floating-point register",
										dest.type(), src.type());
		}
	};

	struct ucvtf : instruction {
		ucvtf(const register_operand& dest, const register_operand& src):
			instruction("ucvtf", def(dest), use(src))
		{ 
			[[unlikely]]
			if (src.type().type_class() == ir::value_type_class::floating_point)
				throw backend_exception("Second operand of ucvtf {}, {} must be a general-purpose register",
										dest.type(), src.type());
			[[unlikely]]
			if (dest.type().type_class() != ir::value_type_class::floating_point)
				throw backend_exception("First operand of ucvtf {}, {} must be a floating-point register",
										dest.type(), src.type());
		}
	};

	struct cfinv : instruction {
		cfinv():
			instruction("cfinv")
		{
            implicitly_writes({register_operand(register_operand::nzcv)});
		}
	};

	struct cas : instruction {
		cas(const register_operand& dst, const register_operand& src, const memory_operand& mem_addr):
			instruction("cas", use(dst), use(src), use(mem_addr))
		{
			as_keep();
		}
	};

	// ATOMICs
    // LDXR{size} {Rt}, [Rn]*/
#define LD_A_XR(name, size) \
    struct name##size : instruction { \
		name##size(const register_operand &dst, const memory_operand &mem): \
			instruction(#name #size, def(dst), use(mem)) \
		{ } \
	};

#define ST_A_XR(name, size) \
	struct name##size : instruction { \
		name##size(const register_operand &status, const register_operand &rt, const memory_operand &mem): \
			instruction(#name #size, def(status), use(rt), use(mem)) \
		{ \
			as_keep(); \
		} \
	};

#define LD_A_XR_VARIANTS(name) \
    LD_A_XR(name, b) \
    LD_A_XR(name, h) \
    LD_A_XR(name, w) \
    LD_A_XR(name,)

#define ST_A_XR_VARIANTS(name) \
    ST_A_XR(name, b) \
    ST_A_XR(name, h) \
    ST_A_XR(name, w) \
    ST_A_XR(name,)

    LD_A_XR_VARIANTS(ldxr);
    LD_A_XR_VARIANTS(ldaxr);

    ST_A_XR_VARIANTS(stxr);
    ST_A_XR_VARIANTS(stlxr);

#define AMO_SIZE_VARIANT(name, suffix_type, suffix_size) \
    struct name##suffix_type##suffix_size : instruction { \
		name##suffix_type##suffix_size(const register_operand &rm, const register_operand &rt, const memory_operand &mem): \
			instruction(#name #suffix_type #suffix_size, use(rm), def(rt), use(mem)) \
		{ \
			as_keep(); \
		} \
    };

#define AMO_SIZE_VARIANTS(name, size) \
    AMO_SIZE_VARIANT(name, , size) \
    AMO_SIZE_VARIANT(name, a, size) \
    AMO_SIZE_VARIANT(name, al, size) \
    AMO_SIZE_VARIANT(name, l, size)

#define AMO_SIZE_VARIANT_HW(name) \
    AMO_SIZE_VARIANTS(name, ) \
    AMO_SIZE_VARIANTS(name, h) \
    AMO_SIZE_VARIANTS(name, w)

#define AMO_SIZE_VARIANT_BHW(name) \
    AMO_SIZE_VARIANT_HW(name) \
    AMO_SIZE_VARIANTS(name, b) \

    AMO_SIZE_VARIANT_BHW(swp);

    AMO_SIZE_VARIANT_BHW(ldadd);

    AMO_SIZE_VARIANT_BHW(ldclr);

    AMO_SIZE_VARIANT_BHW(ldeor);

    AMO_SIZE_VARIANT_BHW(ldset);

    AMO_SIZE_VARIANT_HW(ldsmax);

    AMO_SIZE_VARIANT_HW(ldsmin);

    AMO_SIZE_VARIANT_HW(ldumax);

    AMO_SIZE_VARIANT_HW(ldumin);

	struct msr : instruction {
		msr(const register_operand& sysreg, const register_operand& src):
			instruction("msr", def(sysreg), use(src))
		{ }
	};

	struct mrs : instruction {
		mrs(const register_operand& dest, const register_operand& sysreg):
			instruction("msr", def(dest), use(sysreg))
		{ }
	};

	struct ret : instruction {
		ret(): 
			instruction("ret")
		{ 
			as_keep();
			implicitly_reads({register_operand(register_operand::x0)});
		}
	};

	struct brk : instruction {
		brk(const immediate_operand& imm): 
			instruction("brk", use(imm))
		{ 
			as_keep();
		}
	};


    std::size_t assemble(const char *code, unsigned char **out);

    void free(unsigned char* ptr) const { ks_free(ptr); }

    ~arm64_assembler() {
        ks_close(ks_);
    }
private:
    ks_err status_;
    ks_engine* ks_;

	static reg_or_imm check_immediate(const reg_or_imm& imm, ir::value_type type) { 
		return imm;
	}
};

} // arancini::output::dynamic::arm64
