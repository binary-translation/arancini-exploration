#pragma once

#include <arancini/output/dynamic/arm64/arm64-instruction.h>
#include <arancini/output/dynamic/machine-code-writer.h>

#include <algorithm>
#include <type_traits>
#include <vector>

namespace arancini::output::dynamic::arm64 {

using reg_or_imm = operand_variant<register_operand, immediate_operand>;

class instruction_builder {
  public:
#define ARITH_OP_BASIC(name)                                                   \
    void name(const register_operand &dst, const register_operand &src1,       \
              const reg_or_imm &src2, const std::string &comment = "") {       \
        append(instruction(#name, def(dst), use(src1), use(src2))              \
                   .as_keep()                                                  \
                   .add_comment(comment));                                     \
    }

#define ARITH_OP_SHIFT(name)                                                   \
    void name(const register_operand &dst, const register_operand &src1,       \
              const reg_or_imm &src2, const shift_operand &shift) {            \
        append(instruction(#name, def(dst), use(src1), use(src2), use(shift))  \
                   .as_keep());                                                \
    }

// TODO: refactor everything this way
#define ARITH_OP(name)                                                         \
    ARITH_OP_BASIC(name)                                                       \
    ARITH_OP_SHIFT(name)

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

    // SBCS
    ARITH_OP(sbcs);

    void orr_(const register_operand &dst, const register_operand &src1,
              const reg_or_imm &src2, const std::string &comment = "") {
        append(instruction("orr", def(dst), use(src1), use(src2))
                   .add_comment(comment));
    }

    void and_(const register_operand &dst, const register_operand &src1,
              const reg_or_imm &src2, const std::string &comment = "") {
        append(instruction("and", def(dst), use(src1), use(src2))
                   .add_comment(comment));
    }

    void ands(const register_operand &dst, const register_operand &src1,
              const reg_or_imm &src2, const std::string &comment = "") {
        append(instruction("ands", def(dst), use(src1), use(src2))
                   .as_keep()
                   .add_comment(comment));
    }

    void eor_(const register_operand &dst, const register_operand &src1,
              const reg_or_imm &src2, const std::string &comment = "") {
        append(instruction("eor", def(dst), use(src1), use(src2))
                   .add_comment(comment));
    }

    void not_(const register_operand &dst, const reg_or_imm &src,
              const std::string &comment = "") {
        append(instruction("mvn", def(dst), use(src)).add_comment(comment));
    }

    void neg(const register_operand &dst, const register_operand &src,
             const std::string &comment = "") {
        append(instruction("neg", def(dst), use(src)).add_comment(comment));
    }

    void movn(const register_operand &dst, const immediate_operand &src,
              const shift_operand &shift, const std::string &comment = "") {
        append(instruction("movn", def(dst), use(src), use(shift))
                   .add_comment(comment));
    }

    void movz(const register_operand &dst, const immediate_operand &src,
              const shift_operand &shift, const std::string &comment = "") {
        append(instruction("movz", def(dst), use(src), use(shift))
                   .add_comment(comment));
    }

    void movk(const register_operand &dst, const immediate_operand &src,
              const shift_operand &shift, const std::string &comment = "") {
        append(instruction("movk", use(def(dst)), use(src), use(shift))
                   .add_comment(comment));
    }

    void mov(const register_operand &dst, const reg_or_imm &src,
             const std::string &comment = "") {
        append(instruction("mov", def(dst), use(src)).add_comment(comment));
    }

    void b(const label_operand &dest, const std::string &comment = "") {
        append(instruction("b", use(dest)).add_comment(comment).as_branch());
    }

    void beq(const label_operand &dest, const std::string &comment = "") {
        append(instruction("beq", use(dest)).add_comment(comment).as_branch());
    }

    void bl(const label_operand &dest, const std::string &comment = "") {
        append(instruction("bl", use(dest)).add_comment(comment).as_branch());
    }

    void bne(const label_operand &dest, const std::string &comment = "") {
        append(instruction("bne", use(dest)).add_comment(comment).as_branch());
    }

    void cbz(const register_operand &dest, const label_operand &label,
             const std::string &comment = "") {
        append(instruction("cbz", def(dest), use(label))
                   .as_branch()
                   .add_comment(comment));
    }

    // TODO: check if this allocated correctly
    void cbnz(const register_operand &rt, const label_operand &dest,
              const std::string &comment = "") {
        append(instruction("cbnz", use(rt), use(dest))
                   .add_comment(comment)
                   .as_branch());
    }

    // TODO: handle register_set
    void cmn(const register_operand &dst, const reg_or_imm &src,
             const std::string &comment = "") {
        append(
            instruction("cmn", use(def(dst)), use(src)).add_comment(comment));
    }

    void cmp(const register_operand &dst, const reg_or_imm &src,
             const std::string &comment = "") {
        append(
            instruction("cmp", use(def(dst)), use(src)).add_comment(comment));
    }

    void tst(const register_operand &dst, const reg_or_imm &src,
             const std::string &comment = "") {
        append(
            instruction("tst", use(def(dst)), use(src)).add_comment(comment));
    }

    void lsl(const register_operand &dst, const register_operand &src1,
             const reg_or_imm &src2, const std::string &comment = "") {
        append(instruction("lsl", def(dst), use(src1), use(src2))
                   .add_comment(comment));
    }

    void lsr(const register_operand &dst, const register_operand &src1,
             const reg_or_imm &src2, const std::string &comment = "") {
        append(instruction("lsr", def(dst), use(src1), use(src2))
                   .add_comment(comment));
    }

    void asr(const register_operand &dst, const register_operand &src1,
             const reg_or_imm &src2, const std::string &comment = "") {
        append(instruction("asr", def(dst), use(src1), use(src2))
                   .add_comment(comment));
    }

    void csel(const register_operand &dst, const register_operand &src1,
              const register_operand &src2, const cond_operand &cond,
              const std::string &comment = "") {
        append(instruction("csel", def(dst), use(src1), use(src2), use(cond))
                   .add_comment(comment));
    }

    void cset(const register_operand &dst, const cond_operand &cond,
              const std::string &comment = "") {
        append(instruction("cset", def(dst), use(cond)).add_comment(comment));
    }

    void bfxil(const register_operand &dst, const register_operand &src1,
               const immediate_operand &lsb, const immediate_operand &width,
               const std::string &comment = "") {
        append(
            instruction("bfxil", use(def(dst)), use(src1), use(lsb), use(width))
                .add_comment(comment));
    }

    void bfi(const register_operand &dst, const register_operand &src1,
             const immediate_operand &immr, const immediate_operand &imms,
             const std::string &comment = "") {
        append(
            instruction("bfi", use(def(dst)), use(src1), use(immr), use(imms))
                .as_keep()
                .add_comment(comment));
    }

#define LDR_VARIANTS(name)                                                     \
    void name(const register_operand &dest, const memory_operand &base,        \
              const std::string &comment = "") {                               \
        append(instruction(#name, def(dest), use(base)).add_comment(comment)); \
    }

#define STR_VARIANTS(name)                                                     \
    void name(const register_operand &src, const memory_operand &base,         \
              const std::string &comment = "") {                               \
        append(instruction(#name, use(src), use(base)).add_comment(comment));  \
    }

    LDR_VARIANTS(ldr);
    LDR_VARIANTS(ldrh);
    LDR_VARIANTS(ldrb);

    STR_VARIANTS(str);
    STR_VARIANTS(strh);
    STR_VARIANTS(strb);

    void mul(const register_operand &dest, const register_operand &src1,
             const register_operand &src2, const std::string &comment = "") {
        append(instruction("mul", def(dest), use(src1), use(src2))
                   .add_comment(comment));
    }

    void smulh(const register_operand &dest, const register_operand &src1,
               const register_operand &src2, const std::string &comment = "") {
        append(instruction("smulh", def(dest), use(src1), use(src2))
                   .add_comment(comment));
    }

    void smull(const register_operand &dest, const register_operand &src1,
               const register_operand &src2, const std::string &comment = "") {
        append(instruction("smull", def(dest), use(src1), use(src2))
                   .add_comment(comment));
    }

    void umulh(const register_operand &dest, const register_operand &src1,
               const register_operand &src2, const std::string &comment = "") {
        append(instruction("umulh", def(dest), use(src1), use(src2))
                   .add_comment(comment));
    }

    void umull(const register_operand &dest, const register_operand &src1,
               const register_operand &src2, const std::string &comment = "") {
        append(instruction("umull", def(dest), use(src1), use(src2))
                   .add_comment(comment));
    }

    void fmul(const register_operand &dest, const register_operand &src1,
              const register_operand &src2, const std::string &comment = "") {
        append(instruction("fmul", def(dest), use(src1), use(src2))
                   .add_comment(comment));
    }

    void sdiv(const register_operand &dest, const register_operand &src1,
              const register_operand &src2, const std::string &comment = "") {
        append(instruction("sdiv", def(dest), use(src1), use(src2))
                   .add_comment(comment));
    }

    void fcvtzs(const register_operand &dest, const register_operand &src,
                const std::string &comment = "") {
        append(instruction("fcvtzs", def(dest), use(src)).add_comment(comment));
    }

    void fcvtzu(const register_operand &dest, const register_operand &src,
                const std::string &comment = "") {
        append(instruction("fcvtzu", def(dest), use(src)).add_comment(comment));
    }

    void fcvtas(const register_operand &dest, const register_operand &src,
                const std::string &comment = "") {
        append(instruction("fcvtas", def(dest), use(src)).add_comment(comment));
    }

    void fcvtau(const register_operand &dest, const register_operand &src,
                const std::string &comment = "") {
        append(instruction("fcvtau", def(dest), use(src)).add_comment(comment));
    }

    void scvtf(const register_operand &dest, const register_operand &src,
               const std::string &comment = "") {
        append(instruction("scvtf", def(dest), use(src)).add_comment(comment));
    }

    void ucvtf(const register_operand &dest, const register_operand &src,
               const std::string &comment = "") {
        append(instruction("ucvtf", def(dest), use(src)).add_comment(comment));
    }

    void mrs(const register_operand &dest, const register_operand &src,
             const std::string &comment = "") {
        append(instruction("mrs", use(def(dest)), use(def(src)))
                   .add_comment(comment));
    }

    void msr(const register_operand &dest, const register_operand &src,
             const std::string &comment = "") {
        append(instruction("msr", use(def(dest)), use(def(src)))
                   .add_comment(comment));
    }

    void ret(const std::string &comment = "") {
        append(instruction("ret").add_comment(comment));
    }

    void brk(const immediate_operand &imm, const std::string &comment = "") {
        append(instruction("brk", use(imm)).add_comment(comment));
    }

    void label(const std::string &label, const std::string &comment = "") {
        append(instruction(label_operand(fmt::format("{}:", label)))
                   .add_comment(comment));
    }

    void setz(const register_operand &dst, const std::string &comment = "") {
        append(instruction("cset", def(dst), cond_operand("eq"))
                   .add_comment(comment));
    }

    void sets(const register_operand &dst, const std::string &comment = "") {
        append(instruction("cset", def(dst), cond_operand("lt"))
                   .add_comment(comment));
    }

    void setc(const register_operand &dst, const std::string &comment = "") {
        append(instruction("cset", def(dst), cond_operand("cs"))
                   .add_comment(comment));
    }

    void setcc(const register_operand &dst, const std::string &comment = "") {
        append(instruction("cset", def(dst), cond_operand("cc"))
                   .add_comment(comment));
    }

    void seto(const register_operand &dst, const std::string &comment = "") {
        append(instruction("cset", def(dst), cond_operand("vs"))
                   .add_comment(comment));
    }

    void sxtb(const register_operand &dst, const register_operand &src,
              const std::string &comment = "") {
        append(instruction("sxtb", def(dst), use(src)).add_comment(comment));
    }

    void sxth(const register_operand &dst, const register_operand &src,
              const std::string &comment = "") {
        append(instruction("sxth", def(dst), use(src)).add_comment(comment));
    }

    void sxtw(const register_operand &dst, const register_operand &src,
              const std::string &comment = "") {
        append(instruction("sxtw", def(dst), use(src)).add_comment(comment));
    }

    void uxtb(const register_operand &dst, const register_operand &src,
              const std::string &comment = "") {
        append(instruction("uxtb", def(dst), use(src)).add_comment(comment));
    }

    void uxth(const register_operand &dst, const register_operand &src,
              const std::string &comment = "") {
        append(instruction("uxth", def(dst), use(src)).add_comment(comment));
    }

    void uxtw(const register_operand &dst, const register_operand &src,
              const std::string &comment = "") {
        append(instruction("uxtw", def(dst), use(src)).add_comment(comment));
    }

    void cas(const register_operand &dst, const register_operand &src,
             const memory_operand &mem_addr, const std::string &comment = "") {
        append(instruction("cas", use(dst), use(src), use(mem_addr))
                   .as_keep()
                   .add_comment(comment));
    }

// ATOMICs
// LDXR{size} {Rt}, [Rn]
#define LD_A_XR(name, size)                                                    \
    void name##size(const register_operand &dst, const memory_operand &mem,    \
                    const std::string &comment = "") {                         \
        append(instruction(#name #size, def(dst), use(mem))                    \
                   .add_comment(comment));                                     \
    }

#define ST_A_XR(name, size)                                                    \
    void name##size(const register_operand &status,                            \
                    const register_operand &rt, const memory_operand &mem,     \
                    const std::string &comment = "") {                         \
        append(instruction(#name #size, def(status), use(rt), use(mem))        \
                   .as_keep()                                                  \
                   .add_comment(comment));                                     \
    }

#define LD_A_XR_VARIANTS(name)                                                 \
    LD_A_XR(name, b)                                                           \
    LD_A_XR(name, h)                                                           \
    LD_A_XR(name, w)                                                           \
    LD_A_XR(name, )

#define ST_A_XR_VARIANTS(name)                                                 \
    ST_A_XR(name, b)                                                           \
    ST_A_XR(name, h)                                                           \
    ST_A_XR(name, w)                                                           \
    ST_A_XR(name, )

    LD_A_XR_VARIANTS(ldxr);
    LD_A_XR_VARIANTS(ldaxr);

    ST_A_XR_VARIANTS(stxr);
    ST_A_XR_VARIANTS(stlxr);

#define AMO_SIZE_VARIANT(name, suffix_type, suffix_size)                       \
    void name##suffix_type##suffix_size(const register_operand &rm,            \
                                        const register_operand &rt,            \
                                        const memory_operand &mem) {           \
        append(instruction(#name #suffix_type #suffix_size, use(rm),           \
                           use(def(rt)), use(mem)));                           \
    }

#define AMO_SIZE_VARIANTS(name, size)                                          \
    AMO_SIZE_VARIANT(name, , size)                                             \
    AMO_SIZE_VARIANT(name, a, size)                                            \
    AMO_SIZE_VARIANT(name, al, size)                                           \
    AMO_SIZE_VARIANT(name, l, size)

#define AMO_SIZE_VARIANT_HW(name)                                              \
    AMO_SIZE_VARIANTS(name, )                                                  \
    AMO_SIZE_VARIANTS(name, h)                                                 \
    AMO_SIZE_VARIANTS(name, w)

#define AMO_SIZE_VARIANT_BHW(name)                                             \
    AMO_SIZE_VARIANT_HW(name)                                                  \
    AMO_SIZE_VARIANTS(name, b)

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
#define VOP_ARITH(name)                                                        \
    void name(const std::string &size, const register_operand &dest,           \
              const register_operand &src1, const register_operand &src2) {    \
        append(                                                                \
            instruction(#name "." + size, def(dest), use(src1), use(src2)));   \
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
    // VCMP, VCEQ, VCGE, VCGT: compare, compare equal, compare greater than or
    // equal, compare greater than.
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
    void add(const register_operand &dst, const register_operand &pred,
             const register_operand &src1, const register_operand &src2) {
        append(instruction("add", def(dst), use(pred), use(src1), use(src2)));
    }

    // SVE2
    void sub(const register_operand &dst, const register_operand &pred,
             const register_operand &src1, const register_operand &src2) {
        append(instruction("sub", def(dst), use(pred), use(src1), use(src2)));
    }

    // SVE2
    void ld1(const register_operand &src, const register_operand &pred,
             const memory_operand &addr) {
        append(instruction("ld1", use(src), use(pred), use(addr)));
    }

    void st1(const register_operand &dest, const register_operand &pred,
             const memory_operand &addr) {
        append(instruction("st1", def(dest), use(pred), use(addr)));
    }

    void ptrue(const register_operand &dest) {
        append(instruction("ptrue", def(dest)));
    }

    void insert_separator(const std::string &sep, const std::string &comment) {
        label(sep, comment);
    }

    template <typename... Args>
    void insert_comment(std::string_view format, Args &&...args) {
        append(instruction(fmt::format(
            "// {}", fmt::format(format, std::forward<Args>(args)...))));
    }

    bool has_label(const std::string &label) {
        auto label_str = fmt::format(label, ":");
        auto instr = instructions_;
        return std::any_of(
            instr.rbegin(), instr.rend(),
            [&](const instruction &i) { return i.opcode() == label_str; });
    }

    void allocate();

    void emit(machine_code_writer &writer);

    void dump(std::ostream &os) const;

    std::size_t size() const { return instructions_.size(); }

    using instruction_stream = std::vector<instruction>;

    using instruction_stream_iterator = instruction_stream::iterator;
    using const_instruction_stream_iterator =
        instruction_stream::const_iterator;

    [[nodiscard]]
    instruction_stream_iterator instruction_begin() {
        return instructions_.begin();
    }

    [[nodiscard]]
    const_instruction_stream_iterator instruction_begin() const {
        return instructions_.begin();
    }

    [[nodiscard]]
    const_instruction_stream_iterator instruction_cbegin() const {
        return instructions_.cbegin();
    }

    [[nodiscard]]
    instruction_stream_iterator instruction_end() {
        return instructions_.end();
    }

    [[nodiscard]]
    const_instruction_stream_iterator instruction_end() const {
        return instructions_.end();
    }

    [[nodiscard]]
    const_instruction_stream_iterator instruction_cend() const {
        return instructions_.cend();
    }

  private:
    std::vector<instruction> instructions_;

    void append(const instruction &i) { instructions_.push_back(i); }

    void spill();
};
} // namespace arancini::output::dynamic::arm64
