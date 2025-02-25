#pragma once

#include <arancini/output/dynamic/arm64/arm64-common.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <keystone/keystone.h>

namespace arancini::output::dynamic::arm64 {

class assembler {
public:
    assembler() {
        status_ = ks_open(KS_ARCH_ARM64, KS_MODE_LITTLE_ENDIAN, &ks_);

        if (status_ != KS_ERR_OK)
            throw backend_exception("failed to initialise keystone assembler: {}", ks_strerror(status_));
    }

    std::size_t assemble(const char *code, unsigned char **out);

    void free(unsigned char* ptr) const { ks_free(ptr); }

    [[nodiscard]]
    static instruction add(const register_operand &dst,
                           const register_operand &src1,
                           const reg_or_imm &src2,
                           const shift_operand &shift = {}) {
        return instruction("add", def(dst), use(src1), use(src2), use(shift));
    }

    [[nodiscard]]
    static instruction adds(const register_operand &dst,
                            const register_operand &src1,
                            const reg_or_imm &src2,
                            const shift_operand &shift = {}) {
        return instruction("adds", def(dst), use(src1), use(src2), use(shift))
                           .implicitly_writes({register_operand(register_operand::nzcv)});
    }

    [[nodiscard]]
	static instruction adc(const register_operand &dst,
                           const register_operand &src1,
                           const register_operand &src2) {
        return instruction("adc", def(dst), use(src1), use(src2))
                          .implicitly_reads({register_operand(register_operand::nzcv)});
    }

    [[nodiscard]]
	static instruction adcs(const register_operand &dst,
                            const register_operand &src1,
                            const register_operand &src2) {
        return instruction("adcs", def(dst), use(src1), use(src2))
                          .implicitly_reads({register_operand(register_operand::nzcv)})
                          .implicitly_writes({register_operand(register_operand::nzcv)});
    }

    [[nodiscard]]
	static instruction sub(const register_operand &dst,
                           const register_operand &src1,
                           const reg_or_imm &src2) {
        return instruction("sub", def(dst), use(src1), use(src2));
    }

    [[nodiscard]]
    static instruction sub(const register_operand &dst, const register_operand &src1,
                           const reg_or_imm &src2, const shift_operand &shift)
    {
        return instruction("sub", def(dst), use(src1), use(src2), use(shift));
    }

    [[nodiscard]]
    static instruction subs(const register_operand &dst,
                            const register_operand &src1,
                            const reg_or_imm &src2,
                            const shift_operand &shift = {}) {
        return instruction("subs", def(dst), use(src1), use(src2), use(shift))
                          .implicitly_writes({register_operand(register_operand::nzcv)});
    }

    [[nodiscard]]
	static instruction sbc(const register_operand &dst, const register_operand &src1, const register_operand &src2) {
        return instruction("sbc", def(dst), use(src1), use(src2))
                          .implicitly_reads({register_operand(register_operand::nzcv)});
    }

    [[nodiscard]]
	static instruction sbcs(const register_operand &dst, const register_operand &src1, const register_operand &src2)
    {
        return instruction("sbcs", def(dst), use(src1), use(src2))
                          .implicitly_reads({register_operand(register_operand::nzcv)})
                          .implicitly_writes({register_operand(register_operand::nzcv)});
    }

    [[nodiscard]]
    static instruction orr(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2) {
        return instruction("orr", def(dst), use(src1), use(src2));
    }

    [[nodiscard]]
    static instruction and_(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2) {
        return instruction("and", def(dst), use(src1), use(src2));
    }

    [[nodiscard]]
    static instruction ands(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2) {
        return instruction("ands", def(dst), use(src1), use(src2))
                           .implicitly_writes({register_operand(register_operand::nzcv)});
    }

    [[nodiscard]]
    static instruction eor(const register_operand &dst, const register_operand &src1, const reg_or_imm &src) {
        // TODO: this checks that the immediate is between immr:imms (bits [21:10])
        // However, for 64-bit eor, we have N:immr:imms, which gives us bits [22:10])
        return instruction("eor", def(dst), use(src1), use(src));
    }

    [[nodiscard]]
    static instruction mvn(const register_operand &dst, const reg_or_imm &src,
                           const shift_operand& shift = shift_operand::lsl(0)) {
        return instruction("mvn", def(dst), use(src), use(shift));
    }

    [[nodiscard]]
    static instruction neg(const register_operand &dst, const register_operand& src, const shift_operand& shift = {}) {
        return instruction("neg", def(dst), use(src), use(shift));
    }

    [[nodiscard]]
    static instruction movn(const register_operand &dst, const immediate_operand &src, const shift_operand &shift) {
        check_immediate_size(src, ir::value_type::u16());
        return instruction("movn", def(dst), use(src), use(shift));
    }

    [[nodiscard]]
    static instruction movz(const register_operand &dst, const immediate_operand &src, const shift_operand &shift) {
        check_immediate_size(src, ir::value_type::u16());
        return instruction("movz", def(dst), use(src), use(shift));
    }

    [[nodiscard]]
    static instruction movk(const register_operand &dst, const immediate_operand &src, const shift_operand &shift) {
        check_immediate_size(src, ir::value_type::u16());
        return instruction("movk", use(def(dst)), use(src), use(shift));
    }

    [[nodiscard]]
    static instruction mov(const register_operand &dst, const reg_or_imm &src) {
        check_immediate_size(src, ir::value_type::u(12));
        return instruction("mov", def(dst), use(src));
    }

    [[nodiscard]]
    static instruction b(const label_operand &dest) {
        return instruction("b", use(dest)).as_branch();
    }

    [[nodiscard]]
    static instruction beq(const label_operand &dest) {
        return instruction("beq", use(dest)).as_branch()
                          .implicitly_reads({register_operand(register_operand::nzcv)});
    }

    [[nodiscard]]
    static instruction bl(const label_operand &dest) {
        return instruction("bl", use(dest)).as_branch()
                          .implicitly_reads({register_operand(register_operand::nzcv)});
    }

    [[nodiscard]]
    static instruction bne(const label_operand &dest) {
        return instruction("bne", use(dest)).as_branch()
                           .implicitly_reads({register_operand(register_operand::nzcv)});
    }

    // Check reg == 0 and jump if true
    // Otherwise, continue to the next instruction
    // Does not affect condition flags (can be used to compare-and-branch with 1 instruction)
    [[nodiscard]]
    static instruction cbz(const register_operand &reg, const label_operand &label) {
        // label_refcount_[label.name()]++;
        return instruction("cbz", use(reg), use(label)).as_branch();
    }

    // TODO: check if this allocated correctly
    // Check reg == 0 and jump if false
    // Otherwise, continue to the next instruction
    // Does not affect condition flags (can be used to compare-and-branch with 1 instruction)
    [[nodiscard]]
    static instruction cbnz(const register_operand &rt, const label_operand &dest) {
        // label_refcount_[dest.name()]++;
        return instruction("cbnz", use(rt), use(dest)).as_branch();
    }

    // TODO: handle register_set
    [[nodiscard]]
    static instruction cmn(const register_operand &dst,
                     const reg_or_imm &src) {
        return instruction("cmn", use(dst), use(src))
                      .implicitly_writes({register_operand(register_operand::nzcv)});
    }

    [[nodiscard]]
    static instruction cmp(const register_operand &dst, const reg_or_imm &src) {
        return instruction("cmp", use(dst), use(src))
                      .implicitly_writes({register_operand(register_operand::nzcv)});
    }

    [[nodiscard]]
    static instruction tst(const register_operand &dst, const reg_or_imm &src) {
        return instruction("tst", use(dst), use(src))
                      .implicitly_writes({register_operand(register_operand::nzcv)});
    }

    [[nodiscard]]
    static instruction lsl(const register_operand &dst, const register_operand &src1,
                           const reg_or_imm &src2) {
        auto element_size = dst.type().element_width() <= 32 ? 32 : 64;
        std::size_t bitsize = element_size == 32 ? 5 : 6;
        check_immediate_size(src2, ir::value_type::u(bitsize));
        return instruction("lsl", def(dst), use(src1), use(src2));
    }

    [[nodiscard]]
    static instruction lsr(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2) {
        auto element_size = dst.type().element_width() <= 32 ? 32 : 64;
        std::size_t bitsize = element_size == 32 ? 5 : 6;
        check_immediate_size(src2, ir::value_type::u(bitsize));
        return instruction("lsr", def(dst), use(src1), use(src2));
    }

    [[nodiscard]]
    static instruction asr(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2) {
        auto element_size = dst.type().element_width() <= 32 ? 32 : 64;
        std::size_t bitsize = element_size == 32 ? 5 : 6;
        check_immediate_size(src2, ir::value_type::u(bitsize));
        return instruction("asr", def(dst), use(src1), use(src2));
    }

    [[nodiscard]]
    static instruction extr(const register_operand &dst, const register_operand &src1,
                            const register_operand &src2, const immediate_operand &lsb) {
        std::size_t size = dst.type().element_width() <= 32 ? 5 : 6;
        check_immediate_size(lsb, ir::value_type::u(size));
        return instruction("extr", def(dst), use(src1), use(src2), use(lsb));
    }

    [[nodiscard]]
    static instruction csel(const register_operand &dst, const register_operand &src1,
                            const register_operand &src2, const cond_operand &cond) {
        return instruction("csel", def(dst), use(src1), use(src2), use(cond))
                          .implicitly_reads({register_operand(register_operand::x0)});
    }

    [[nodiscard]]
    static instruction cset(const register_operand &dst, const cond_operand &cond) {
        return instruction("cset", def(dst), use(cond))
                           .implicitly_reads({register_operand(register_operand::x0)});
    }

    [[nodiscard]]
    static instruction bfxil(const register_operand &dst,
                             const register_operand &src1,
                             const immediate_operand &lsb,
                             const immediate_operand &width)
    {
        auto element_size = dst.type().element_width() <= 32 ? 32 : 64;
        std::size_t bitsize = element_size == 32 ? 5 : 6;
        auto fits = immediate_operand::fits(lsb.value(), ir::value_type::u(bitsize));
        fits &= immediate_operand::fits(width.value(), ir::value_type::u(element_size - lsb.value()));

        [[unlikely]]
        if (!fits)
            throw backend_exception("Invalid width immediate {} for BFXIL static instruction must fit into [1,{}] for lsb {}",
                                    width, element_size - lsb.value(), lsb);

        return instruction("bfxil", use(def(dst)), use(src1), use(lsb), use(width));
    }

    [[nodiscard]]
    static instruction ubfx(const register_operand &dst, const register_operand &src1,
                            const immediate_operand &lsb, const immediate_operand &width)
    {
        auto element_size = dst.type().element_width() <= 32 ? 32 : 64;
        std::size_t bitsize = element_size == 32 ? 5 : 6;
        auto fits = immediate_operand::fits(lsb.value(), ir::value_type::u(bitsize));
        fits &= immediate_operand::fits(width.value(), ir::value_type::u(element_size - lsb.value()));

        [[unlikely]]
        if (!fits)
            throw backend_exception("Invalid width immediate {} for UBFX static instruction must fit into [1,{}] for lsb {}",
                                    width, element_size - lsb.value(), lsb);
        return instruction("ubfx", def(dst), use(src1), use(lsb), use(width));
    }

    [[nodiscard]]
    static instruction bfi(const register_operand &dst, const register_operand &src1,
                           const immediate_operand &lsb, const immediate_operand &width)
    {
        auto element_size = dst.type().element_width() <= 32 ? 32 : 64;
        std::size_t bitsize = element_size == 32 ? 5 : 6;
        auto fits = immediate_operand::fits(lsb.value(), ir::value_type::u(bitsize));
        fits &= immediate_operand::fits(width.value(), ir::value_type::u(element_size - lsb.value()));

        [[unlikely]]
        if (!fits)
            throw backend_exception("Invalid width immediate {} for BFI static instruction must fit into [1,{}] for lsb {}",
                                    width, element_size - lsb.value(), lsb);
        return instruction("bfi", use(def(dst)), use(src1), use(lsb), use(width));
    }

#define LDR_VARIANTS(name) \
    [[nodiscard]] \
    static instruction name(const register_operand &dest, \
             const memory_operand &base) { \
        return instruction(#name, def(dest), use(base)); \
    } \

#define STR_VARIANTS(name) \
    [[nodiscard]] \
    static instruction name(const register_operand &src, \
              const memory_operand &base) { \
        return instruction(#name, use(src), use(base)).as_keep(); \
    } \

    LDR_VARIANTS(ldr);
    LDR_VARIANTS(ldrh);
    LDR_VARIANTS(ldrb);

    STR_VARIANTS(str);
    STR_VARIANTS(strh);
    STR_VARIANTS(strb);

    [[nodiscard]]
    static instruction mul(const register_operand &dest,
             const register_operand &src1,
             const register_operand &src2) {
        return instruction("mul", def(dest), use(src1), use(src2));
    }

    [[nodiscard]]
    static instruction smulh(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return instruction("smulh", def(dest), use(src1), use(src2));
    }

    [[nodiscard]]
    static instruction smull(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return instruction("smull", def(dest), use(src1), use(src2));
    }

    [[nodiscard]]
    static instruction umulh(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return instruction("umulh", def(dest), use(src1), use(src2));
    }

    [[nodiscard]]
    static instruction umull(const register_operand &dest, const register_operand &src1, const register_operand &src2) {
        return instruction("umull", def(dest), use(src1), use(src2));
    }

    [[nodiscard]]
    static instruction sdiv(const register_operand &dest, const register_operand &src1, const register_operand &src2) {
        return instruction("sdiv", def(dest), use(src1), use(src2));
    }

    [[nodiscard]]
    static instruction udiv(const register_operand &dest, const register_operand &src1, const register_operand &src2) {
        return instruction("udiv", def(dest), use(src1), use(src2));
    }

    [[nodiscard]]
    static instruction fmul(const register_operand &dest, const register_operand &src1, const register_operand &src2) {
        [[unlikely]]
        if (src1.type().type_class() != ir::value_type_class::floating_point)
            throw backend_exception("Second operand of fmul must be floating-point instead of {}", src1.type());
        [[unlikely]]
        if (src2.type().type_class() != ir::value_type_class::floating_point)
            throw backend_exception("Third operand of fmul must be floating-point instead of {}", src1.type());
        return instruction("fmul", def(dest), use(src1), use(src2));
    }

    [[nodiscard]]
    static instruction fmov(const register_operand &dest, const reg_or_imm &src) {
        // TODO: missing checks
        check_immediate_size(src, ir::value_type::f(8));
        return instruction("fmov", def(dest), use(src));
    }

    [[nodiscard]]
    static instruction fcvt(const register_operand &dest, const register_operand &src) {
        [[unlikely]]
        if (src.type().type_class() != ir::value_type_class::floating_point &&
            dest.type().type_class() != ir::value_type_class::floating_point)
        {
            throw backend_exception("Either the first or the second operand of fcvt {}, {} must be a floating point register",
                                    dest.type(), src.type());
        }
        return instruction("fcvt", def(dest), use(src));
    }

    [[nodiscard]]
    static instruction fcvtzs(const register_operand &dest, const register_operand &src) {
        [[unlikely]]
        if (src.type().type_class() != ir::value_type_class::floating_point)
            throw backend_exception("The second operand of fcvtsz {}, {} must be a floating point register",
                                    dest.type(), src.type());
        [[unlikely]]
        if (dest.type().type_class() == ir::value_type_class::floating_point)
            throw backend_exception("The first operand of fcvtsz {}, {} must be a GPR",
                                    dest.type(), src.type());
        return instruction("fcvtzs", def(dest), use(src));
    }

    [[nodiscard]]
    static instruction fcvtzu(const register_operand &dest, const register_operand &src) {
        [[unlikely]]
        if (src.type().type_class() != ir::value_type_class::floating_point)
            throw backend_exception("The second operand of fcvtzu {}, {} must be a floating point register",
                                    dest.type(), src.type());
        [[unlikely]]
        if (dest.type().type_class() == ir::value_type_class::floating_point)
            throw backend_exception("The first operand of fcvtzu {}, {} must be a GPR",
                                    dest.type(), src.type());
        return instruction("fcvtzu", def(dest), use(src));
    }

    [[nodiscard]]
    static instruction fcvtas(const register_operand &dest, const register_operand &src) {
        [[unlikely]]
        if (src.type().type_class() != ir::value_type_class::floating_point)
            throw backend_exception("The second operand of fcvtas {}, {} must be a floating point register",
                                    dest.type(), src.type());
        [[unlikely]]
        if (dest.type().type_class() == ir::value_type_class::floating_point)
            throw backend_exception("The first operand of fcvtas {}, {} must be a GPR",
                                    dest.type(), src.type());
        return instruction("fcvtas", def(dest), use(src));
    }

    [[nodiscard]]
    static instruction fcvtau(const register_operand &dest, const register_operand &src) {
        [[unlikely]]
        if (src.type().type_class() != ir::value_type_class::floating_point)
            throw backend_exception("The second operand of fcvtau {}, {} must be a floating point register",
                                    dest.type(), src.type());
        [[unlikely]]
        if (dest.type().type_class() == ir::value_type_class::floating_point)
            throw backend_exception("The first operand of fcvtau {}, {} must be a GPR",
                                    dest.type(), src.type());
        return instruction("fcvtau", def(dest), use(src));
    }

    // TODO: this also has an immediate variant
    [[nodiscard]]
    static instruction fcmp(const register_operand &dest, const register_operand &src) {
        [[unlikely]]
        if (src.type().type_class() != ir::value_type_class::floating_point ||
            src.type().type_class() != ir::value_type_class::floating_point)
        {
            throw backend_exception("The first and second operand of fcmp {}, {} must be a floating point register",
                                    dest.type(), src.type());
        }

        return instruction("fcmp", def(dest), use(src))
                      .implicitly_writes({register_operand(register_operand::nzcv)});
    }

    [[nodiscard]]
    static instruction scvtf(const register_operand &dest, const register_operand &src) {
        return instruction("scvtf", def(dest), use(src));
    }

    [[nodiscard]]
    static instruction ucvtf(const register_operand &dest,
               const register_operand &src) {
        return instruction("ucvtf", def(dest), use(src));
    }

    [[nodiscard]]
    static instruction mrs(const register_operand &dest, const register_operand &src) {
        return instruction("mrs", def(dest), use(src));
    }

    [[nodiscard]]
    static instruction msr(const register_operand &dest, const register_operand &src) {
        return instruction("msr", def(dest), use(src));
    }

    [[nodiscard]]
    static instruction ret() {
        return instruction("ret").as_keep().implicitly_reads({register_operand(register_operand::x0)});
    }

    [[nodiscard]]
    static instruction brk(const immediate_operand &imm) {
        return instruction("brk", use(imm));
    }

    void label(const label_operand &label) {
        // if (!labels_.count(label.name()) {
        //     labels_.insert(label.name();
        //     instruction(label);
        //     return;
        // }
        // logger.debug("Label {} already inserted\nCurrent labels:\n{}",
        //              label, fmt::format("{}", fmt::join(labels_.begin(), labels_.end(), "\n"));
    }

    [[nodiscard]]
    static instruction cfinv() {
        return instruction("cfinv");
    }

    [[nodiscard]]
    static instruction sxtb(const register_operand &dst, const register_operand &src) {
        return instruction("sxtb", def(dst), use(src));
    }

    [[nodiscard]]
    static instruction sxth(const register_operand &dst, const register_operand &src) {
        return instruction("sxth", def(dst), use(src));
    }

    [[nodiscard]]
    static instruction sxtw(const register_operand &dst, const register_operand &src) {
        return instruction("sxtw", def(dst), use(src));
    }

    [[nodiscard]]
    static instruction uxtb(const register_operand &dst, const register_operand &src) {
        return instruction("uxtb", def(dst), use(src));
    }

    [[nodiscard]]
    static instruction uxth(const register_operand &dst, const register_operand &src) {
        return instruction("uxth", def(dst), use(src));
    }

    [[nodiscard]]
    static instruction uxtw(const register_operand &dst, const register_operand &src) {
        return instruction("uxtw", def(dst), use(src));
    }

    [[nodiscard]]
    static instruction cas(const register_operand &dst, const register_operand &src,
                     const memory_operand &mem_addr) {
        return instruction("cas", use(dst), use(src), use(mem_addr)).as_keep();
    }

// ATOMICs
    // LDXR{size} {Rt}, [Rn]
#define LD_A_XR(name, size) \
    [[nodiscard]] \
    static instruction name##size(const register_operand &dst, const memory_operand &mem) { \
        return instruction(#name#size, def(dst), use(mem)); \
    }

#define ST_A_XR(name, size) \
    [[nodiscard]] \
    static instruction name##size(const register_operand &status, const register_operand &rt, const memory_operand &mem) { \
        return instruction(#name#size, def(status), use(rt), use(mem)).as_keep(); \
    }

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
    [[nodiscard]] \
    static instruction name##suffix_type##suffix_size(const register_operand &rm, const register_operand &rt, const memory_operand &mem) { \
        return instruction(#name#suffix_type#suffix_size, use(rm), def(rt), use(mem)).as_keep(); \
    }

#define AMO_SIZE_VARIANTS(name, size) \
    AMO_SIZE_VARIANT(name, , size) \
    AMO_SIZE_VARIANT(name, a, size) \
    AMO_SIZE_VARIANT(name, al, size) \
    AMO_SIZE_VARIANT(name, l, size)

#define AMO_SIZE_VARIANT_BHW(name) \
        AMO_SIZE_VARIANTS(name, ) \
        AMO_SIZE_VARIANTS(name, h) \
        AMO_SIZE_VARIANTS(name, w) \
        AMO_SIZE_VARIANTS(name, b) \

    AMO_SIZE_VARIANT_BHW(swp);

    AMO_SIZE_VARIANT_BHW(ldadd);

    AMO_SIZE_VARIANT_BHW(ldclr);

    AMO_SIZE_VARIANT_BHW(ldeor);

    AMO_SIZE_VARIANT_BHW(ldset);

    AMO_SIZE_VARIANT_BHW(ldsmax);

    AMO_SIZE_VARIANT_BHW(ldsmin);

    AMO_SIZE_VARIANT_BHW(ldumax);

    AMO_SIZE_VARIANT_BHW(ldumin);

// NEON Vectors
// NOTE: these should be built only if there is no support for SVE/SVE2
//
// Otherwise, we should just use SVE2 operations directly
#define VOP_ARITH(name) \
    [[nodiscard]] \
    static instruction name(const std::string &size, const register_operand &dest, const register_operand &src1, const register_operand &src2) { \
        return instruction(#name "." + size, def(dest), use(src1), use(src2)); \
    }

    // vector additions
    VOP_ARITH(vadd);
    VOP_ARITH(vqadd);
    VOP_ARITH(vhadd);
    VOP_ARITH(vrhadd);

    // vector subtractions
    VOP_ARITH(vsub);
    VOP_ARITH(vqsub);
    VOP_ARITH(vhsub);

    // vector multiplication
    // TODO: some not included
    VOP_ARITH(vmul);
    VOP_ARITH(vmla);
    VOP_ARITH(vmls);

    // vector absolute value
    VOP_ARITH(vabs);
    VOP_ARITH(vqabs);

    // vector negation
    VOP_ARITH(vneg);
    VOP_ARITH(vqneg);

    // vector round float
    VOP_ARITH(vrnd);
    VOP_ARITH(vrndi);
    VOP_ARITH(vrnda);

    // pairwise addition
    VOP_ARITH(vpadd);
    VOP_ARITH(vpaddl);

    // vector shifts
    // shift left, shift right, shift left long.
    VOP_ARITH(vshl);
    VOP_ARITH(vshr);
    VOP_ARITH(vshll);

    // vector comparisons
    // VCMP, VCEQ, VCGE, VCGT: compare, compare equal, compare greater than or equal, compare greater than.
    VOP_ARITH(vcmp);
    VOP_ARITH(vceq);
    VOP_ARITH(vcge);
    VOP_ARITH(vcgt);

    // min-max
    VOP_ARITH(vmin);
    VOP_ARITH(vmax);

    // reciprocal-sqrt
    VOP_ARITH(vrecpe);
    VOP_ARITH(vrsqrte);

// SVE2
    // SVE2 version
    [[nodiscard]]
    static instruction add(const register_operand &dst,
             const register_operand &pred,
             const register_operand &src1,
             const register_operand &src2) {
        return instruction("add", def(dst), use(pred), use(src1), use(src2));
    }

    // SVE2
    [[nodiscard]]
    static instruction sub(const register_operand &dst,
             const register_operand &pred,
             const register_operand &src1,
             const register_operand &src2) {
        return instruction("sub", def(dst), use(pred), use(src1), use(src2));
    }

    // SVE2
    [[nodiscard]]
    static instruction ld1(const register_operand &src,
             const register_operand &pred,
             const memory_operand &addr) {
        return instruction("ld1", use(src), use(pred), use(addr));
    }

    [[nodiscard]]
    static instruction st1(const register_operand &dest,
             const register_operand &pred,
             const memory_operand &addr) {
        return instruction("st1", def(dest), use(pred), use(addr));
    }

    [[nodiscard]]
    static instruction ptrue(const register_operand &dest) {
        return instruction("ptrue", def(dest));
    }


    ~assembler() {
        ks_close(ks_);
    }

    static void check_immediate_size(const reg_or_imm& op, const ir::value_type& type) {
        if (!std::holds_alternative<immediate_operand>(op.get())) return;
        immediate_operand imm(op);
        if (!immediate_operand::fits(imm.value(), type))
            throw backend_exception("immediate {} cannot fit into {} (strict requirement)",
                                    imm, type);
    }
private:
    ks_err status_;
    ks_engine* ks_;

    const bool supports_lse = false;
};

} // namespace arancini::output::dynamic::arm64

