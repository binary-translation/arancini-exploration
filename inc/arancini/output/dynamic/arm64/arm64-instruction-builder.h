#pragma once

#include <keystone/keystone.h>

#include <arancini/ir/port.h>
#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <vector>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

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

    ~assembler() {
        ks_close(ks_);
    }
public:
    ks_err status_;
    ks_engine* ks_;
};

class register_sequence final {
public:
    // TODO: do we need this?
    register_sequence() = default;

    register_sequence(const register_operand& reg):
        regs_{reg}
    { }

    register_sequence(std::initializer_list<register_operand> regs):
        regs_(regs)
    { }

    template <typename It>
    register_sequence(It begin, It end):
        regs_(begin, end)
    { }

    operator register_operand() const {
        [[unlikely]]
        if (regs_.size() > 1)
            throw backend_exception("Accessing register set of {} registers as single register",
                                    regs_.size());
        return regs_[0];
    }

    operator std::vector<register_operand>() const {
        return regs_;
    }

    [[nodiscard]]
    register_operand& operator[](std::size_t i) { return regs_[i]; }

    [[nodiscard]]
    const register_operand& operator[](std::size_t i) const { return regs_[i]; }

    [[nodiscard]]
    register_operand& front() { return regs_[0]; }

    [[nodiscard]]
    const register_operand& front() const { return regs_[0]; }

    [[nodiscard]]
    register_operand& back() { return regs_[regs_.size()]; }

    [[nodiscard]]
    const register_operand& back() const { return regs_[regs_.size()]; }

    [[nodiscard]]
    std::size_t size() const { return regs_.size(); }

    void push_back(const register_operand& reg) { regs_.push_back(reg); }

    void push_back(register_operand&& reg) { regs_.push_back(std::move(reg)); }
private:
    std::vector<register_operand> regs_;
};

class virtual_register_allocator final {
public:
    [[nodiscard]]
    register_sequence allocate([[maybe_unused]] ir::value_type type);

    void reset() { next_vreg_ = 33; }
private:
    std::size_t next_vreg_ = 33; // TODO: formalize this

    bool base_representable(const ir::value_type& type) {
        return type.width() <= 64; // TODO: formalize this
    }
};

class instruction_builder final {
public:
    struct immediates_upgrade_policy final {
        friend instruction_builder;

        reg_or_imm operator()(const reg_or_imm& op) {
            if (std::holds_alternative<register_operand>(op.get()))
                return op;

            const immediate_operand& imm = op;
            return builder_->move_immediate(imm.value(), imm_type_, reg_type_);
        }
    protected:
        immediates_upgrade_policy(instruction_builder* builder, ir::value_type imm_type, ir::value_type reg_type):
            builder_(builder),
            imm_type_(imm_type),
            reg_type_(reg_type)
        { }
    private:
        instruction_builder* builder_;
        ir::value_type imm_type_;
        ir::value_type reg_type_;
    };

    struct immediates_strict_policy final {
        friend instruction_builder;

        reg_or_imm operator()(const reg_or_imm& op) {
            if (std::holds_alternative<register_operand>(op.get()))
                return op;

            const immediate_operand& imm = op;
            if (!immediate_operand::fits(imm.value(), imm_type_))
                throw backend_exception("immediate {} cannot fit into {} (strict requirement)");

            return op;
        }
    protected:
        immediates_strict_policy(instruction_builder* builder, ir::value_type imm_type, ir::value_type reg_type):
            imm_type_(imm_type)
        { }

        immediates_strict_policy(ir::value_type imm_type):
            imm_type_(imm_type)
        { }

        ir::value_type imm_type_;
    };

	instruction& add(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        return append(instruction("add", def(dst), use(src1), use(src2)));
    }

    instruction& add(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2,
                      const shift_operand &shift) {
        return append(instruction("add", def(dst), use(src1), use(src2), use(shift)));
    }

	instruction& adds(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        return append(instruction("adds", def(dst), use(src1), use(src2))
                      .implicitly_writes({register_operand(register_operand::nzcv)}));
    }

    instruction& adds(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2,
                      const shift_operand &shift) {
        return append(instruction("adds", def(dst), use(src1), use(src2), use(shift))
                      .implicitly_writes({register_operand(register_operand::nzcv)}));
    }

	instruction& adcs(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        return append(instruction("adcs", def(dst), use(src1), use(src2))
                      .implicitly_reads({register_operand(register_operand::nzcv)})
                      .implicitly_writes({register_operand(register_operand::nzcv)}));
    }

	instruction& sub(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        return append(instruction("sub", def(dst), use(src1), use(src2)));
    }

    instruction& sub(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2,
                      const shift_operand &shift) {
        return append(instruction("sub", def(dst), use(src1), use(src2), use(shift)));
    }

	instruction& subs(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        return append(instruction("subs", def(dst), use(src1), use(src2))
                      .implicitly_writes({register_operand(register_operand::nzcv)}));
    }

    instruction& subs(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2,
                      const shift_operand &shift) {
        return append(instruction("subs", def(dst), use(src1), use(src2), use(shift))
                      .implicitly_writes({register_operand(register_operand::nzcv)}));
    }

	instruction& sbc(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        return append(instruction("sbc", def(dst), use(src1), use(src2))
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

    instruction& sbc(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2,
                      const shift_operand &shift) {
        return append(instruction("sbc", def(dst), use(src1), use(src2), use(shift))
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

	instruction& sbcs(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        return append(instruction("sbcs", def(dst), use(src1), use(src2))
                      .implicitly_reads({register_operand(register_operand::nzcv)})
                      .implicitly_writes({register_operand(register_operand::nzcv)}));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& orr_(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        // TODO: this checks that the immediate is between immr:imms (bits [21:10])
        // However, for 64-bit orr, we have N:immr:imms, which gives us bits [22:10])
        ImmediatesPolicy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        return append(instruction("orr", def(dst), use(src1), use(policy(src2))));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& and_(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        // TODO: this checks that the immediate is between immr:imms (bits [21:10])
        // However, for 64-bit and, we have N:immr:imms, which gives us bits [22:10])
        ImmediatesPolicy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        return append(instruction("and", def(dst), use(src1), use(policy(src2))));
    }

    // TODO: refactor this; there should be only a single version and the comment should be removed
    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& ands(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        // TODO: this checks that the immediate is between immr:imms (bits [21:10])
        // However, for 64-bit ands, we have N:immr:imms, which gives us bits [22:10])
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        return append(instruction("ands", def(dst), use(src1), use(immediates_policy(src2)))
                      .implicitly_writes({register_operand(register_operand::nzcv)}));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& eor_(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2) {
        // TODO: this checks that the immediate is between immr:imms (bits [21:10])
        // However, for 64-bit eor, we have N:immr:imms, which gives us bits [22:10])
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        return append(instruction("eor", def(dst), use(src1), use(immediates_policy(src2))));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& not_(const register_operand &dst, const reg_or_imm &src) {
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 6), dst.type());
        return append(instruction("mvn", def(dst), use(immediates_policy(src))));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& neg(const register_operand &dst, const register_operand &src) {
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 6), dst.type());
        return append(instruction("neg", def(dst), use(immediates_policy(src))));
    }

    instruction& movn(const register_operand &dst,
              const immediate_operand &src,
              const shift_operand &shift) {
        return append(instruction("movn", def(dst), use(src), use(shift)));
    }

    instruction& movz(const register_operand &dst,
              const immediate_operand &src,
              const shift_operand &shift) {
        immediates_strict_policy immediates_policy(ir::value_type(ir::value_type_class::unsigned_integer, 16));
        return append(instruction("movz", def(dst), use(immediates_policy(src)), use(shift)));
    }

    instruction& movk(const register_operand &dst,
              const immediate_operand &src,
              const shift_operand &shift) {
        immediates_strict_policy immediates_policy(ir::value_type(ir::value_type_class::unsigned_integer, 16));
        return append(instruction("movk", use(def(dst)), use(immediates_policy(src)), use(shift)));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& mov(const register_operand &dst, const reg_or_imm &src) {
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        return append(instruction("mov", def(dst), use(immediates_policy(src))));
    }

    instruction& b(label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(instruction("b", use(dest)).as_branch());
    }

    instruction& beq(label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(instruction("beq", use(dest)).as_branch()
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

    instruction& bl(label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(instruction("bl", use(dest)).as_branch()
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

    instruction& bne(label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(instruction("bne", use(dest)).as_branch()
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

    // Check reg == 0 and jump if true
    // Otherwise, continue to the next instruction
    // Does not affect condition flags (can be used to compare-and-branch with 1 instruction)
    instruction& cbz(const register_operand &reg, const label_operand &label) {
        label_refcount_[label.name()]++;
        return append(instruction("cbz", use(reg), use(label)).as_branch());
    }

    // TODO: check if this allocated correctly
    // Check reg == 0 and jump if false
    // Otherwise, continue to the next instruction
    // Does not affect condition flags (can be used to compare-and-branch with 1 instruction)
    instruction& cbnz(const register_operand &rt, const label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(instruction("cbnz", use(rt), use(dest)).as_branch());
    }

    // TODO: handle register_set
    instruction& cmn(const register_operand &dst,
                     const reg_or_imm &src) {
        return append(instruction("cmn", use(dst), use(src))
                      .implicitly_writes({register_operand(register_operand::nzcv)}));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& cmp(const register_operand &dst, const reg_or_imm &src) {
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        return append(instruction("cmp", use(dst), use(immediates_policy(src)))
                      .implicitly_writes({register_operand(register_operand::nzcv)}));
    }

    instruction& tst(const register_operand &dst, const reg_or_imm &src) {
        return append(instruction("tst", use(dst), use(src))
                      .implicitly_writes({register_operand(register_operand::nzcv)}));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& lsl(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2) {
        std::size_t size = dst.type().element_width() <= 32 ? 4 : 6;
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), dst.type());
        return append(instruction("lsl", def(dst), use(src1), use(immediates_policy(src2))));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& lsr(const register_operand &dst,
                     const register_operand &src1,
                     const reg_or_imm &src2) {
        std::size_t size = dst.type().element_width() <= 32 ? 5 : 6;
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), dst.type());
        return append(instruction("lsr", def(dst), use(src1), use(immediates_policy(src2))));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& asr(const register_operand &dst,
                     const register_operand &src1,
                     const reg_or_imm &src2) {
        std::size_t size = dst.type().element_width() <= 32 ? 5 : 6;
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), dst.type());
        return append(instruction("asr", def(dst), use(src1), use(immediates_policy(src2))));
    }

    instruction& extr(const register_operand &dst,
                      const register_operand &src1,
                      const register_operand &src2,
                      const immediate_operand &shift) {
        std::size_t size = dst.type().element_width() <= 32 ? 5 : 6;
        immediates_strict_policy policy(ir::value_type(ir::value_type_class::unsigned_integer, size));
        return append(instruction("extr", def(dst), use(src1), use(src2), use(policy(shift))));
    }

    instruction& csel(const register_operand &dst,
                      const register_operand &src1,
                      const register_operand &src2,
                      const cond_operand &cond) {
        return append(instruction("csel", def(dst), use(src1), use(src2), use(cond))
                      .implicitly_reads({register_operand(register_operand::x0)}));
    }

    instruction& cset(const register_operand &dst,
                      const cond_operand &cond) {
        return append(instruction("cset", def(dst), use(cond))
                      .implicitly_reads({register_operand(register_operand::x0)}));
    }

    instruction& bfxil(const register_operand &dst,
                       const register_operand &src1,
                       const immediate_operand &lsb,
                       const immediate_operand &width)
    {
        auto element_size = dst.type().element_width() <= 32 ? 32 : 64;
        std::size_t bitsize = element_size == 32 ? 5 : 6;
        immediates_strict_policy policy(ir::value_type(ir::value_type_class::unsigned_integer, bitsize));
        if (width.value() > element_size - lsb.value())
            throw backend_exception("Invalid width immediate {} for BFXIL instruction must fit into [1,{}] for lsb",
                                    width, element_size - lsb.value(), lsb);
        return append(instruction("bfxil", use(def(dst)), use(src1), use(policy(lsb)), use(width)));
    }

    instruction& ubfx(const register_operand &dst,
                      const register_operand &src1,
                      const immediate_operand &lsb,
                      const immediate_operand &width)
    {
        auto element_size = dst.type().element_width() <= 32 ? 32 : 64;
        std::size_t bitsize = element_size == 32 ? 5 : 6;
        immediates_strict_policy policy(ir::value_type(ir::value_type_class::unsigned_integer, bitsize));
        if (width.value() > element_size - lsb.value())
            throw backend_exception("Invalid width immediate {} for UBFX instruction must fit into [1,{}] for lsb",
                                    width, element_size - lsb.value(), lsb);
        return append(instruction("ubfx", def(dst), use(src1), use(policy(lsb)), use(width)));
    }

    instruction& bfi(const register_operand &dst,
                     const register_operand &src1,
                     const immediate_operand &lsb,
                     const immediate_operand &width)
    {
        auto element_size = dst.type().element_width() <= 32 ? 32 : 64;
        std::size_t bitsize = element_size == 32 ? 5 : 6;
        immediates_strict_policy policy(ir::value_type(ir::value_type_class::unsigned_integer, bitsize));
        if (width.value() > element_size - lsb.value())
            throw backend_exception("Invalid width immediate {} for BFI instruction must fit into [1,{}] for lsb {}",
                                    width, element_size - lsb.value(), lsb);
        return append(instruction("bfi", use(def(dst)), use(src1), use(lsb), use(width)));
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

    instruction& fmul(const register_operand &dest,
                      const register_operand &src1,
                      const register_operand &src2) {
        if (src1.type().type_class() != ir::value_type_class::floating_point)
            throw backend_exception("Second operand of fmul must be floating-point instead of {}",
                                    src1.type());
        if (src2.type().type_class() != ir::value_type_class::floating_point)
            throw backend_exception("Third operand of fmul must be floating-point instead of {}",
                                    src1.type());
        return append(instruction("fmul", def(dest), use(src1), use(src2)));
    }

    instruction& fdiv(const register_operand &dest,
                      const register_operand &src1,
                      const register_operand &src2) {
        return append(instruction("fdiv", def(dest), use(src1), use(src2)));
    }

    instruction& fmov(const register_operand &dest,
                      const register_operand &src) {
        if (src.type().type_class() != ir::value_type_class::floating_point &&
            dest.type().type_class() != ir::value_type_class::floating_point)
        {
            throw backend_exception("Either the first or the second operand of fmov {}, {} must be a floating point register",
                                    dest.type(), src.type());
        }
        return append(instruction("fmov", def(dest), use(src)));
    }

    instruction& fmov(const register_operand &dest,
                      const immediate_operand &src) {
        if (dest.type().type_class() != ir::value_type_class::floating_point) {
            throw backend_exception("The first operand of fmov {}, {} must be a floating point register",
                                    dest.type(), src.type());
        }
        return append(instruction("fmov", def(dest), use(src)));
    }

    instruction& fcvt(const register_operand &dest,
                      const register_operand &src) {
        if (src.type().type_class() != ir::value_type_class::floating_point &&
            dest.type().type_class() != ir::value_type_class::floating_point)
        {
            throw backend_exception("Either the first or the second operand of fcvt {}, {} must be a floating point register",
                                    dest.type(), src.type());
        }
        return append(instruction("fcvt", def(dest), use(src)));
    }

    instruction& fcvtzs(const register_operand &dest, const register_operand &src) {
        if (src.type().type_class() != ir::value_type_class::floating_point)
            throw backend_exception("The second operand of fcvtsz {}, {} must be a floating point register",
                                    dest.type(), src.type());
        if (dest.type().type_class() == ir::value_type_class::floating_point)
            throw backend_exception("The first operand of fcvtsz {}, {} must be a GPR",
                                    dest.type(), src.type());
        return append(instruction("fcvtzs", def(dest), use(src)));
    }

    instruction& fcvtzu(const register_operand &dest, const register_operand &src) {
        if (src.type().type_class() != ir::value_type_class::floating_point)
            throw backend_exception("The second operand of fcvtzu {}, {} must be a floating point register",
                                    dest.type(), src.type());
        if (dest.type().type_class() == ir::value_type_class::floating_point)
            throw backend_exception("The first operand of fcvtzu {}, {} must be a GPR",
                                    dest.type(), src.type());
        return append(instruction("fcvtzu", def(dest), use(src)));
    }

    instruction& fcvtas(const register_operand &dest, const register_operand &src) {
        if (src.type().type_class() != ir::value_type_class::floating_point)
            throw backend_exception("The second operand of fcvtas {}, {} must be a floating point register",
                                    dest.type(), src.type());
        if (dest.type().type_class() == ir::value_type_class::floating_point)
            throw backend_exception("The first operand of fcvtas {}, {} must be a GPR",
                                    dest.type(), src.type());
        return append(instruction("fcvtas", def(dest), use(src)));
    }

    instruction& fcvtau(const register_operand &dest, const register_operand &src) {
        if (src.type().type_class() != ir::value_type_class::floating_point)
            throw backend_exception("The second operand of fcvtau {}, {} must be a floating point register",
                                    dest.type(), src.type());
        if (dest.type().type_class() == ir::value_type_class::floating_point)
            throw backend_exception("The first operand of fcvtau {}, {} must be a GPR",
                                    dest.type(), src.type());
        return append(instruction("fcvtau", def(dest), use(src)));
    }

    // TODO: this also has an immediate variant
    instruction& fcmp(const register_operand &dest, const register_operand &src) {
        if (src.type().type_class() != ir::value_type_class::floating_point ||
            src.type().type_class() != ir::value_type_class::floating_point)
        {
            throw backend_exception("The first and second operand of fcmp {}, {} must be a floating point register",
                                    dest.type(), src.type());
        }

        return append(instruction("fcmp", def(dest), use(src))
                      .implicitly_writes({register_operand(register_operand::nzcv)}));
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
        return append(instruction("mrs", def(dest), use(src)));
    }

    instruction& msr(const register_operand &dest,
                     const register_operand &src) {
        return append(instruction("msr", def(dest), use(src)));
    }

    instruction& ret() {
        return append(instruction("ret").as_keep().implicitly_reads({register_operand(register_operand::x0)}));
    }

    instruction& brk(const immediate_operand &imm) {
        return append(instruction("brk", use(imm)));
    }

    void label(const label_operand &label) {
        if (!labels_.count(label.name())) {
            labels_.insert(label.name());
            append(instruction(label));
            return;
        }
        logger.debug("Label {} already inserted\nCurrent labels:\n{}",
                     label, fmt::format("{}", fmt::join(labels_.begin(), labels_.end(), "\n")));
    }

	instruction& setz(const register_operand &dst) {
        return append(instruction("cset", def(dst), cond_operand::eq())
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

	instruction& sets(const register_operand &dst) {
        return append(instruction("cset", def(dst), cond_operand::lt())
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

	instruction& setc(const register_operand &dst) {
        return append(instruction("cset", def(dst), cond_operand::cs())
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

	instruction& setcc(const register_operand &dst) {
        return append(instruction("cset", def(dst), cond_operand::cc())
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }
	instruction& seto(const register_operand &dst) {
        return append(instruction("cset", def(dst), cond_operand::vs())
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
        return append(instruction(#name#suffix_type#suffix_size, use(rm), def(rt), use(mem)).as_keep()); \
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

    template <typename... Args>
    void insert_comment(std::string_view format, Args&&... args) {
        append(instruction(fmt::format("// {}", fmt::format(format, std::forward<Args>(args)...))));
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

    virtual_register_allocator& register_allocator() { return vreg_alloc_; }

    const virtual_register_allocator& register_allocator() const { return vreg_alloc_; }

    [[nodiscard]]
    inline std::size_t get_min_bitsize(unsigned long long imm) {
        return value_types::base_type.element_width() - __builtin_clzll(imm|1);
    }

    template <typename T, std::enable_if_t<std::is_arithmetic<T>::value, int> = 0>
    reg_or_imm move_immediate(T imm, ir::value_type imm_type, ir::value_type reg_type) {
        [[unlikely]]
        if (imm_type.is_vector() || imm_type.element_width() > value_types::base_type.element_width())
            throw backend_exception("Attempting to move immediate {:#x} into unsupported immediate type {}",
                                     imm, imm_type);

        auto immediate = util::bit_cast_zeros<unsigned long long>(imm);
        std::size_t actual_size = get_min_bitsize(immediate);

        if (actual_size < imm_type.element_width()) {
            logger.debug("Immediate {:#x} fits within {} (actual size = {})\n",
                          imm, imm_type, actual_size);
            return immediate_operand(imm, imm_type);
        }

        return move_to_register(imm, reg_type);
     }

    template <typename T, std::enable_if_t<std::is_arithmetic<T>::value, int> = 0>
    reg_or_imm move_immediate(T imm, ir::value_type imm_type) {
        ir::value_type reg_type;
        if (imm_type.element_width() < 32)
            reg_type = ir::value_type(imm_type.type_class(), 32);
        else if (imm_type.element_width() > 32)
            reg_type = ir::value_type(imm_type.type_class(), 64);

        return move_immediate(imm, imm_type, reg_type);
    }

    template <typename T, std::enable_if_t<std::is_arithmetic<T>::value, int> = 0>
    register_operand move_to_register(T imm, ir::value_type type) {
        // Sanity checks
        static_assert (sizeof(T) <= sizeof(std::uint64_t),
                       "Attempting to move immediate requiring more than 64-bits into register");
        static_assert (sizeof(std::uint64_t) <= sizeof(unsigned long long),
                       "ARM DBT expects unsigned long long to be at least as large as 64-bits");

        [[unlikely]]
        if (type.is_vector())
            throw backend_exception("Cannot move immediate {} into vector type {}", imm, type);

        // Convert to unsigned long long so that clzll can be used
        // 1s in sizeof(unsinged long long) - sizeof(imm) upper bits
        auto immediate = util::bit_cast_zeros<unsigned long long>(imm);

        // Check the actual size of the value
        std::size_t actual_size = get_min_bitsize(immediate);
        if (actual_size > type.width()) {
            logger.warn("Converting value of size {} to size {} by truncation\n",
                        actual_size, type.element_width());
            actual_size = type.element_width();
        }

        // Can be moved in one go
        // TODO: implement optimization to support more immediates via shifts
        if (actual_size < 12) {
            insert_comment("Move immediate {:#x} directly as < 12-bits", immediate);
            auto reg = vreg_alloc_.allocate(type);
            std::uint64_t mask = (1ULL << actual_size) - 1;
            mov<immediates_strict_policy>(reg, immediate & mask);
            return reg;
        }

        // Determine how many 16-bit chunks we need to move
        std::size_t move_count = actual_size / 16 + (actual_size % 16 != 0);
        logger.debug("Moving value {:#x} requires {} 16-bit moves\n", immediate, move_count);

        // Can be moved in multiple operations
        // NOTE: this assumes that we're only working with 64-bit registers or smaller
        auto reg = vreg_alloc_.allocate(type);
        insert_comment("Move immediate {:#x} > 12-bits with sequence of movz/movk operations", immediate);
        movz(reg, immediate & 0xFFFF, shift_operand(shift_operand::shift_type::lsl, 0));
        for (std::size_t i = 1; i < move_count; ++i) {
            movk(reg, immediate >> (i * 16) & 0xFFFF, shift_operand(shift_operand::shift_type::lsl, i * 16));
        }

        return reg;
    }

    void clear() {
        instructions_.clear();
        vreg_alloc_.reset();
        label_refcount_.clear();
        labels_.clear();
    }
private:
    assembler asm_;
	std::vector<instruction> instructions_;
    virtual_register_allocator vreg_alloc_;
    std::unordered_map<std::string, std::size_t> label_refcount_;
    std::unordered_set<std::string> labels_;

	instruction& append(const instruction &i) {
        instructions_.push_back(i);
        return instructions_.back();
    }

    void spill();
};

struct atomic_block {
    atomic_block(instruction_builder& builder, std::size_t block_id, const memory_operand& mem_addr):
        mem_addr_(mem_addr),
        loop_label_(fmt::format("loop_{}", block_id)),
        success_label_(fmt::format("success_{}", block_id)),
        builder_(&builder)
    {
    }

    void start_atomic_block(const register_operand& data_reg) {
        builder_->label(loop_label_);
        switch (data_reg.type().element_width()) {
        case 1:
        case 8:
            builder_->ldxrb(data_reg, mem_addr_).add_comment("load atomically");
            break;
        case 16:
            builder_->ldxrh(data_reg, mem_addr_).add_comment("load atomically");
            break;
        case 32:
        case 64:
            builder_->ldxr(data_reg, mem_addr_).add_comment("load atomically");
            break;
        default:
            throw backend_exception("Cannot load atomically values of type {}",
                                    data_reg.type());
        }
    }

    void end_atomic_block(const register_operand& status_reg, const register_operand& src_reg) {
        switch (src_reg.type().element_width()) {
        case 1:
        case 8:
            builder_->stxrb(status_reg, src_reg, mem_addr_).add_comment("store if not failure");
            break;
        case 16:
            builder_->stxrh(status_reg, src_reg, mem_addr_).add_comment("store if not failure");
            break;
        case 32:
        case 64:
            builder_->stxr(status_reg, src_reg, mem_addr_).add_comment("store if not failure");
            break;
        default:
            throw backend_exception("Cannot store atomically values of type {}",
                                    src_reg.type());
        }
        // Compare and also set flags for later
        builder_->cbz(status_reg, success_label_).add_comment("== 0 represents success storing");
        builder_->b(loop_label_).add_comment("loop until failure or success");
        builder_->label(success_label_);
    }

    memory_operand mem_addr_;
    label_operand loop_label_;
    label_operand success_label_;
    instruction_builder* builder_;
};

} // namespace arancini::output::dynamic::arm64

