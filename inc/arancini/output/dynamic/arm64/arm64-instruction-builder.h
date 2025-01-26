#pragma once

#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <vector>
#include <algorithm>
#include <type_traits>

namespace arancini::output::dynamic::arm64 {

class instruction_builder {
public:

#define ARITH_OP_BASIC(name) \
	instruction& name(const register_operand &dst, \
                      const register_operand &src1, \
                      const reg_or_imm &src2) { \
        return append(instruction(#name, def(dst), use(src1), use(src2)) \
                      .implicitly_writes({register_operand(register_operand::nzcv)})); \
    }

#define ARITH_OP_SHIFT(name) \
    instruction& name(const register_operand &dst, \
                      const register_operand &src1, \
                      const reg_or_imm &src2, \
                      const shift_operand &shift) { \
        return append(instruction(#name, def(dst), use(src1), use(src2), use(shift)) \
                      .implicitly_writes({register_operand(register_operand::nzcv)})); \
    }

// TODO: refactor everything this way
#define ARITH_OP(name) \
    ARITH_OP_BASIC(name) \
    ARITH_OP_SHIFT(name) \

    // ADD
    ARITH_OP(add);

    // ADDS
    ARITH_OP(adds);

    // ADCS
    ARITH_OP(adcs);

    // SUB
    ARITH_OP(sub);

    // SUBS
    ARITH_OP(subs);

    // SBC
    ARITH_OP(sbc);

    // SBCS
    ARITH_OP(sbcs);

    instruction& orr_(const register_operand &dst,
              const register_operand &src1,
              const reg_or_imm &src2) {
        return append(instruction("orr", def(dst), use(src1), use(src2)));
    }

    instruction& and_(const register_operand &dst,
              const register_operand &src1,
              const reg_or_imm &src2) {
        return append(instruction("and", def(dst), use(src1), use(src2)));
    }

    // TODO: refactor this; there should be only a single version and the comment should be removed
    instruction& ands(const register_operand &dst,
              const register_operand &src1,
              const reg_or_imm &src2) {
        return append(instruction("ands", def(dst), use(src1), use(src2))
                      .implicitly_writes({register_operand(register_operand::nzcv)}));
    }

    instruction& eor_(const register_operand &dst,
              const register_operand &src1,
              const reg_or_imm &src2) {
        return append(instruction("eor", def(dst), use(src1), use(src2)));
    }

    instruction& not_(const register_operand &dst, const reg_or_imm &src) {
        return append(instruction("mvn", def(dst), use(src)));
    }

    instruction& neg(const register_operand &dst, const register_operand &src) {
        return append(instruction("neg", def(dst), use(src)));
    }

    instruction& movn(const register_operand &dst,
              const immediate_operand &src,
              const shift_operand &shift) {
        return append(instruction("movn", def(dst), use(src), use(shift)));
    }

    instruction& movz(const register_operand &dst,
              const immediate_operand &src,
              const shift_operand &shift) {
        return append(instruction("movz", def(dst), use(src), use(shift)));
    }

    instruction& movk(const register_operand &dst,
              const immediate_operand &src,
              const shift_operand &shift) {
        return append(instruction("movk", use(def(dst)), use(src), use(shift)));
    }

    instruction& mov(const register_operand &dst, const reg_or_imm &src) {
        return append(instruction("mov", def(dst), use(src)));
    }

    instruction& b(const label_operand &dest) {
        return append(instruction("b", use(dest)).as_branch());
    }

    instruction& beq(const label_operand &dest) {
        return append(instruction("beq", use(dest)).as_branch());
    }

    instruction& bl(const label_operand &dest) {
        return append(instruction("bl", use(dest)).as_branch());
    }

    instruction& bne(const label_operand &dest) {
        return append(instruction("bne", use(dest)).as_branch());
    }

    // Check reg == 0 and jump if true
    // Otherwise, continue to the next instruction
    // Does not affect condition flags (can be used to compare-and-branch with 1 instruction)
    instruction& cbz(const register_operand &reg, const label_operand &label) {
        return append(instruction("cbz", use(reg), use(label)).as_branch());
    }

    // TODO: check if this allocated correctly
    instruction& cbnz(const register_operand &rt, const label_operand &dest) {
        return append(instruction("cbnz", use(rt), use(dest)).as_branch());
    }

    // TODO: handle register_set
    instruction& cmn(const register_operand &dst,
             const reg_or_imm &src) {
        return append(instruction("cmn", use(def(dst)), use(src)));
    }

    instruction& cmp(const register_operand &dst,
             const reg_or_imm &src) {
        return append(instruction("cmp", use(dst), use(src))
                      .implicitly_writes({register_operand(register_operand::nzcv)}));
    }

    instruction& tst(const register_operand &dst,
             const reg_or_imm &src) {
        return append(instruction("tst", use(def(dst)), use(src)));
    }

    instruction& lsl(const register_operand &dst,
             const register_operand &src1,
             const reg_or_imm &src2) {
        return append(instruction("lsl", def(dst), use(src1), use(src2)));
    }

    instruction& lsr(const register_operand &dst,
             const register_operand &src1,
             const reg_or_imm &src2) {
        return append(instruction("lsr", def(dst), use(src1), use(src2)));
    }

    instruction& asr(const register_operand &dst,
             const register_operand &src1,
             const reg_or_imm &src2) {
        return append(instruction("asr", def(dst), use(src1), use(src2)));
    }

    instruction& csel(const register_operand &dst,
              const register_operand &src1,
              const register_operand &src2,
              const cond_operand &cond) {
        return append(instruction("csel", def(dst), use(src1), use(src2), use(cond)));
    }

    instruction& cset(const register_operand &dst,
              const cond_operand &cond) {
        return append(instruction("cset", def(dst), use(cond)));
    }

    instruction& bfxil(const register_operand &dst,
               const register_operand &src1,
               const immediate_operand &lsb,
               const immediate_operand &width) {
        return append(instruction("bfxil", use(def(dst)), use(src1), use(lsb), use(width)));
    }

    instruction& bfi(const register_operand &dst,
             const register_operand &src1,
             const immediate_operand &immr,
             const immediate_operand &imms) {
        return append(instruction("bfi", use(def(dst)), use(src1), use(immr), use(imms)));
    }


#define LDR_VARIANTS(name) \
    instruction& name(const register_operand &dest, \
             const memory_operand &base) { \
        return append(instruction(#name, def(dest), use(base))); \
    } \

#define STR_VARIANTS(name) \
    instruction& name(const register_operand &src, \
              const memory_operand &base) { \
        return append(instruction(#name, use(src), use(base)).as_keep()); \
    } \

    LDR_VARIANTS(ldr);
    LDR_VARIANTS(ldrh);
    LDR_VARIANTS(ldrb);

    STR_VARIANTS(str);
    STR_VARIANTS(strh);
    STR_VARIANTS(strb);

    instruction& mul(const register_operand &dest,
             const register_operand &src1,
             const register_operand &src2) {
        return append(instruction("mul", def(dest), use(src1), use(src2)));
    }

    instruction& smulh(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(instruction("smulh", def(dest), use(src1), use(src2)));
    }

    instruction& smull(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(instruction("smull", def(dest), use(src1), use(src2)));
    }

    instruction& umulh(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(instruction("umulh", def(dest), use(src1), use(src2)));
    }

    instruction& umull(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(instruction("umull", def(dest), use(src1), use(src2)));
    }

    instruction& fmul(const register_operand &dest,
              const register_operand &src1,
              const register_operand &src2) {
        return append(instruction("fmul", def(dest), use(src1), use(src2)));
    }

    instruction& sdiv(const register_operand &dest,
              const register_operand &src1,
              const register_operand &src2) {
        return append(instruction("sdiv", def(dest), use(src1), use(src2)));
    }

    instruction& udiv(const register_operand &dest,
              const register_operand &src1,
              const register_operand &src2) {
        return append(instruction("udiv", def(dest), use(src1), use(src2)));
    }

    instruction& fcvtzs(const register_operand &dest,
                const register_operand &src) {
        return append(instruction("fcvtzs", def(dest), use(src)));
    }

    instruction& fcvtzu(const register_operand &dest,
                const register_operand &src) {
        return append(instruction("fcvtzu", def(dest), use(src)));
    }

    instruction& fcvtas(const register_operand &dest,
                const register_operand &src) {
        return append(instruction("fcvtas", def(dest), use(src)));
    }

    instruction& fcvtau(const register_operand &dest,
                const register_operand &src) {
        return append(instruction("fcvtau", def(dest), use(src)));
    }

    instruction& scvtf(const register_operand &dest,
               const register_operand &src) {
        return append(instruction("scvtf", def(dest), use(src)));
    }

    instruction& ucvtf(const register_operand &dest,
               const register_operand &src) {
        return append(instruction("ucvtf", def(dest), use(src)));
    }

    instruction& mrs(const register_operand &dest,
             const register_operand &src) {
        return append(instruction("mrs", use(def(dest)), use(def(src))));
    }

    instruction& msr(const register_operand &dest,
             const register_operand &src) {
        return append(instruction("msr", use(def(dest)), use(def(src))));
    }

    instruction& ret() {
        return append(instruction("ret").implicitly_reads({register_operand(register_operand::x0)}));
    }

    instruction& brk(const immediate_operand &imm) {
        return append(instruction("brk", use(imm)));
    }

    instruction& label(const std::string &label) {
        return append(instruction(label_operand(fmt::format("{}:", label))));
    }

	instruction& setz(const register_operand &dst) {
        return append(instruction("cset", def(dst), cond_operand("eq"))
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

	instruction& sets(const register_operand &dst) {
        return append(instruction("cset", def(dst), cond_operand("lt"))
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

	instruction& setc(const register_operand &dst) {
        return append(instruction("cset", def(dst), cond_operand("cs"))
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

	instruction& setcc(const register_operand &dst) {
        return append(instruction("cset", def(dst), cond_operand("cc"))
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }
	instruction& seto(const register_operand &dst) {
        return append(instruction("cset", def(dst), cond_operand("vs"))
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

    instruction& cfinv() {
        return append(instruction("cfinv"));
    }

    instruction& sxtb(const register_operand &dst, const register_operand &src) {
        return append(instruction("sxtb", def(dst), use(src)));
    }

    instruction& sxth(const register_operand &dst, const register_operand &src) {
        return append(instruction("sxth", def(dst), use(src)));
    }

    instruction& sxtw(const register_operand &dst, const register_operand &src) {
        return append(instruction("sxtw", def(dst), use(src)));
    }

    instruction& uxtb(const register_operand &dst, const register_operand &src) {
        return append(instruction("uxtb", def(dst), use(src)));
    }

    instruction& uxth(const register_operand &dst, const register_operand &src) {
        return append(instruction("uxth", def(dst), use(src)));
    }

    instruction& uxtw(const register_operand &dst, const register_operand &src) {
        return append(instruction("uxtw", def(dst), use(src)));
    }

    instruction& cas(const register_operand &dst, const register_operand &src,
                     const memory_operand &mem_addr) {
        return append(instruction("cas", use(dst), use(src), use(mem_addr)).as_keep());
    }

// ATOMICs
    // LDXR{size} {Rt}, [Rn]
#define LD_A_XR(name, size) \
    instruction& name##size(const register_operand &dst, const memory_operand &mem) { \
        return append(instruction(#name#size, def(dst), use(mem))); \
    }

#define ST_A_XR(name, size) \
    instruction& name##size(const register_operand &status, const register_operand &rt, const memory_operand &mem) { \
        return append(instruction(#name#size, def(status), use(rt), use(mem)).as_keep()); \
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
    instruction& name##suffix_type##suffix_size(const register_operand &rm, const register_operand &rt, const memory_operand &mem) { \
        return append(instruction(#name#suffix_type#suffix_size, use(rm), use(def(rt)), use(mem))); \
    }

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

// NEON Vectors
// NOTE: these should be built only if there is no support for SVE/SVE2
//
// Otherwise, we should just use SVE2 operations directly
#define VOP_ARITH(name) \
    instruction& name(const std::string &size, const register_operand &dest, const register_operand &src1, const register_operand &src2) { \
        return append(instruction(#name "." + size, def(dest), use(src1), use(src2))); \
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
    instruction& add(const register_operand &dst,
             const register_operand &pred,
             const register_operand &src1,
             const register_operand &src2) {
        return append(instruction("add", def(dst), use(pred), use(src1), use(src2)));
    }

    // SVE2
    instruction& sub(const register_operand &dst,
             const register_operand &pred,
             const register_operand &src1,
             const register_operand &src2) {
        return append(instruction("sub", def(dst), use(pred), use(src1), use(src2)));
    }

    // SVE2
    instruction& ld1(const register_operand &src,
             const register_operand &pred,
             const memory_operand &addr) {
        return append(instruction("ld1", use(src), use(pred), use(addr)));
    }

    instruction& st1(const register_operand &dest,
             const register_operand &pred,
             const memory_operand &addr) {
        return append(instruction("st1", def(dest), use(pred), use(addr)));
    }

    instruction& ptrue(const register_operand &dest) {
        return append(instruction("ptrue", def(dest)));
    }

    instruction& insert_separator(const std::string &sep) { return label(sep); }

    template <typename... Args>
    void insert_comment(std::string_view format, Args&&... args) {
        append(instruction(fmt::format("// {}", fmt::format(format, std::forward<Args>(args)...))));
    }

    // TODO: maybe broken
    template <typename... Args>
    bool has_label(const std::string& format, Args&&... args) {
        auto label = fmt::format(fmt::format("{}:", format), std::forward<Args>(args)...);
        return std::any_of(instructions_.rbegin(), instructions_.rend(),
                            [&](const instruction &inst) {
                                return inst.opcode() == label;
                            });
    }

	void allocate();

	void emit(machine_code_writer &writer);

	void dump(std::ostream &os) const;

    std::size_t size() const { return instructions_.size(); }

    using instruction_stream = std::vector<instruction>;

    using instruction_stream_iterator = instruction_stream::iterator;
    using const_instruction_stream_iterator = instruction_stream::const_iterator;

    [[nodiscard]]
    instruction_stream_iterator instruction_begin() { return instructions_.begin(); }

    [[nodiscard]]
    const_instruction_stream_iterator instruction_begin() const { return instructions_.begin(); }

    [[nodiscard]]
    const_instruction_stream_iterator instruction_cbegin() const { return instructions_.cbegin(); }

    [[nodiscard]]
    instruction_stream_iterator instruction_end() { return instructions_.end(); }

    [[nodiscard]]
    const_instruction_stream_iterator instruction_end() const { return instructions_.end(); }

    [[nodiscard]]
    const_instruction_stream_iterator instruction_cend() const { return instructions_.cend(); }
private:
	std::vector<instruction> instructions_;

	instruction& append(const instruction &i) {
        instructions_.push_back(i);
        return instructions_.back();
    }

    void spill();
};
} // namespace arancini::output::dynamic::arm64

