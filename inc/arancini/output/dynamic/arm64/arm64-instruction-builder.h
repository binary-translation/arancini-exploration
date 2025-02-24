#pragma once

#include <keystone/keystone.h>

#include <arancini/ir/port.h>
#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/arm64/arm64-assembler.h>

#include <vector>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace arancini::output::dynamic::arm64 {

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

	instruction& add(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2,
                      const shift_operand &shift = {}) {
        if (std::holds_alternative<register_operand>(src2.get()))
            return append(assembler::add(dst, src1, register_operand(src2), shift));
        return append(assembler::add(dst, src1, immediate_operand(src2), shift));
    }


    instruction& adds(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2,
                      const shift_operand &shift = {}) {
        if (std::holds_alternative<register_operand>(src2.get()))
            return append(assembler::adds(dst, src1, register_operand(src2), shift));
        return append(assembler::adds(dst, src1, immediate_operand(src2), shift));
    }

	instruction& adc(const register_operand &dst,
                      const register_operand &src1,
                      const register_operand &src2) {
        return append(assembler::adc(dst, src1, register_operand(src2)));
    }

	instruction& adcs(const register_operand &dst,
                      const register_operand &src1,
                      const register_operand &src2) {
        return append(assembler::adcs(dst, src1, src2));
    }

    instruction& sub(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2,
                      const shift_operand &shift = {}) {
        if (std::holds_alternative<register_operand>(src2.get()))
            return append(assembler::sub(dst, src1, register_operand(src2), shift));
        return append(assembler::sub(dst, src1, immediate_operand(src2), shift));
    }

    instruction& subs(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2,
                      const shift_operand &shift = {}) {
        if (std::holds_alternative<register_operand>(src2.get()))
            return append(assembler::subs(dst, src1, register_operand(src2), shift));
        return append(assembler::subs(dst, src1, immediate_operand(src2), shift));
    }

	instruction& sbc(const register_operand &dst,
                      const register_operand &src1,
                      const register_operand &src2) {
        return append(assembler::sbc(dst, src1, register_operand(src2)));
    }

	instruction& sbcs(const register_operand &dst,
                      const register_operand &src1,
                      const register_operand &src2) {
        return append(assembler::sbc(dst, src1, src2));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& orr_(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        // TODO: this checks that the immediate is between immr:imms (bits [21:10])
        // However, for 64-bit orr, we have N:immr:imms, which gives us bits [22:10])
        auto src_conv = ImmediatesPolicy(this, ir::value_type::u(12), dst.type())(src2);
        return append(assembler::orr(dst, src1, src_conv));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& and_(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        // TODO: this checks that the immediate is between immr:imms (bits [21:10])
        // However, for 64-bit and, we have N:immr:imms, which gives us bits [22:10])
        auto src_conv = ImmediatesPolicy(this, ir::value_type::u(12), dst.type())(src2);
        return append(assembler::and_(dst, src1, src_conv));
    }

    // TODO: refactor this; there should be only a single version and the comment should be removed
    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& ands(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        // TODO: this checks that the immediate is between immr:imms (bits [21:10])
        // However, for 64-bit ands, we have N:immr:imms, which gives us bits [22:10])
        auto src_conv = ImmediatesPolicy(this, ir::value_type::u(12), dst.type())(src2);
        return append(assembler::ands(dst, src1, src_conv));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& eor_(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2) {
        // TODO: this checks that the immediate is between immr:imms (bits [21:10])
        // However, for 64-bit eor, we have N:immr:imms, which gives us bits [22:10])
        auto src_conv = ImmediatesPolicy(this, ir::value_type::u(12), dst.type())(src2);
        return append(assembler::eor_(dst, src1, src_conv));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& not_(const register_operand &dst, const reg_or_imm &src) {
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 6), dst.type());
        return append(assembler::mvn(dst, immediates_policy(src)));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& neg(const register_operand &dst, const register_operand &src) {
        return append(assembler::neg(dst, src));
    }

    instruction& movn(const register_operand &dst,
                      const immediate_operand &src,
                      const shift_operand &shift) {
        return append(assembler::movn(dst, src, shift));
    }

    instruction& movz(const register_operand &dst,
                      const immediate_operand &src,
                      const shift_operand &shift) {
        return append(assembler::movz(dst, src, shift));
    }

    instruction& movk(const register_operand &dst,
                      const immediate_operand &src,
                      const shift_operand &shift) {
        return append(assembler::movk(dst, src, shift));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& mov(const register_operand &dst, const reg_or_imm &src) {
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        auto src_conv = immediates_policy(src);
        if (std::holds_alternative<register_operand>(src_conv.get()))
            return append(assembler::mov(dst, register_operand(src_conv)));
        return append(assembler::mov(dst, immediate_operand(src_conv)));
    }

    instruction& b(const label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(assembler::b(dest).as_branch());
    }

    instruction& beq(const label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(assembler::beq(dest).as_branch()
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

    instruction& bl(const label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(assembler::bl(dest));
    }

    instruction& bne(const label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(assembler::bne(dest));
    }

    // Check reg == 0 and jump if true
    // Otherwise, continue to the next instruction
    // Does not affect condition flags (can be used to compare-and-branch with 1 instruction)
    instruction& cbz(const register_operand &reg, const label_operand &label) {
        label_refcount_[label.name()]++;
        return append(assembler::cbz(reg, label));
    }

    // TODO: check if this allocated correctly
    // Check reg == 0 and jump if false
    // Otherwise, continue to the next instruction
    // Does not affect condition flags (can be used to compare-and-branch with 1 instruction)
    instruction& cbnz(const register_operand &rt, const label_operand &dest) {
        label_refcount_[dest.name()]++;
        return append(assembler::cbnz(rt, dest));
    }

    // TODO: handle register_set
    instruction& cmn(const register_operand &dst,
                     const reg_or_imm &src) {
        return append(assembler::cmn(dst, src));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& cmp(const register_operand &dst, const reg_or_imm &src) {
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        return append(assembler::cmp(dst, immediates_policy(src)));
    }

    instruction& tst(const register_operand &dst, const reg_or_imm &src) {
        if (std::holds_alternative<register_operand>(src.get()))
            return append(assembler::tst(dst, register_operand(src)));
        return append(assembler::tst(dst, immediate_operand(src)));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& lsl(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2) {
        std::size_t size = dst.type().element_width() <= 32 ? 4 : 6;
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), dst.type());
        return append(assembler::lsl(dst, src1, immediates_policy(src2)));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& lsr(const register_operand &dst,
                     const register_operand &src1,
                     const reg_or_imm &src2) {
        std::size_t size = dst.type().element_width() <= 32 ? 5 : 6;
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), dst.type());
        return append(assembler::lsr(dst, src1, immediates_policy(src2)));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& asr(const register_operand &dst,
                     const register_operand &src1,
                     const reg_or_imm &src2) {
        std::size_t size = dst.type().element_width() <= 32 ? 5 : 6;
        ImmediatesPolicy immediates_policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), dst.type());
        return append(assembler::asr(dst, src1, immediates_policy(src2)));
    }

    instruction& extr(const register_operand &dst,
                      const register_operand &src1,
                      const register_operand &src2,
                      const immediate_operand &shift) {
        return append(assembler::extr(dst, src1, src2, shift));
    }

    instruction& csel(const register_operand &dst,
                      const register_operand &src1,
                      const register_operand &src2,
                      const cond_operand &cond) {
        return append(assembler::csel(dst, src1, src2, cond));
    }

    instruction& cset(const register_operand &dst,
                      const cond_operand &cond) {
        return append(assembler::cset(dst, cond));
    }

    instruction& bfxil(const register_operand &dst,
                       const register_operand &src1,
                       const immediate_operand &lsb,
                       const immediate_operand &width)
    {
        return append(assembler::bfxil(dst, src1, lsb, width));
    }

    instruction& ubfx(const register_operand &dst,
                      const register_operand &src1,
                      const immediate_operand &lsb,
                      const immediate_operand &width)
    {
        return append(assembler::ubfx(dst, src1, lsb, width));
    }

    instruction& bfi(const register_operand &dst,
                     const register_operand &src1,
                     const immediate_operand &lsb,
                     const immediate_operand &width)
    {
        return append(assembler::bfi(dst, src1, lsb, width));
    }

    instruction& load(const register_operand& dest, const memory_operand& memory) {
        if (dest.type().element_width() <= 8)
            return append(assembler::ldrb(dest, memory));
        if (dest.type().element_width() <= 16)
            return append(assembler::ldrh(dest, memory));
        return append(assembler::ldr(dest, memory));
    }

    instruction& store(const register_operand& source, const memory_operand& memory) {
        if (source.type().element_width() <= 8)
            return append(assembler::strb(source, memory));
        if (source.type().element_width() <= 16)
            return append(assembler::strh(source, memory));
        return append(assembler::str(source, memory));
    }

    instruction& mul(const register_operand &dest,
             const register_operand &src1,
             const register_operand &src2) {
        return append(assembler::mul(dest, src1, src2));
    }

    instruction& smulh(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(assembler::smulh(dest, src1, src2));
    }

    instruction& smull(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(assembler::smull(dest, src1, src2));
    }

    instruction& umulh(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(assembler::umulh(dest, src1, src2));
    }

    instruction& umull(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(assembler::umull(dest, src1, src2));
    }

    instruction& sdiv(const register_operand &dest,
              const register_operand &src1,
              const register_operand &src2) {
        return append(assembler::sdiv(dest, src1, src2));
    }

    instruction& udiv(const register_operand &dest,
              const register_operand &src1,
              const register_operand &src2) {
        return append(assembler::udiv(dest, src1, src2));
    }

    instruction& fmul(const register_operand &dest,
                      const register_operand &src1,
                      const register_operand &src2) {
        return append(assembler::fmul(dest, src1, src2));
    }

    instruction& fmov(const register_operand &dest, const reg_or_imm &src) {
        return append(assembler::fmov(dest, src));
    }

    instruction& fcvt(const register_operand &dest, const register_operand &src) {
        return append(assembler::fcvt(dest, src));
    }

    instruction& fcvtzs(const register_operand &dest, const register_operand &src) {
        return append(assembler::fcvtzs(dest, src));
    }

    instruction& fcvtzu(const register_operand &dest, const register_operand &src) {
        return append(assembler::fcvtzu(dest, src));
    }

    instruction& fcvtas(const register_operand &dest, const register_operand &src) {
        return append(assembler::fcvtas(dest, src));
    }

    instruction& fcvtau(const register_operand &dest, const register_operand &src) {
        return append(assembler::fcvtau(dest, src));
    }

    // TODO: this also has an immediate variant
    instruction& fcmp(const register_operand &dest, const register_operand &src) {
        return append(assembler::fcmp(dest, src));
    }

    instruction& scvtf(const register_operand &dest,
               const register_operand &src) {
        return append(assembler::scvtf(dest, src));
    }

    instruction& ucvtf(const register_operand &dest,
               const register_operand &src) {
        return append(assembler::ucvtf(dest, src));
    }

    instruction& mrs(const register_operand &dest,
             const register_operand &src) {
        return append(assembler::mrs(dest, src));
    }

    instruction& msr(const register_operand &dest,
                     const register_operand &src) {
        return append(assembler::msr(dest, src));
    }

    instruction& ret() {
        return append(assembler::ret());
    }

    instruction& brk(const immediate_operand &imm) {
        return append(assembler::brk(imm));
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
        return append(assembler::cset(dst, cond_operand::eq()));
    }

	instruction& sets(const register_operand &dst) {
        return append(assembler::cset(dst, cond_operand::lt()));
    }

	instruction& setc(const register_operand &dst) {
        return append(assembler::cset(dst, cond_operand::cs()));
    }

	instruction& setcc(const register_operand &dst) {
        return append(assembler::cset(dst, cond_operand::cc()));
    }
	instruction& seto(const register_operand &dst) {
        return append(assembler::cset(dst, cond_operand::vs()));
    }

    instruction& cfinv() {
        return append(assembler::cfinv());
    }

    instruction& sxtb(const register_operand &dst, const register_operand &src) {
        return append(assembler::sxtb(dst, src));
    }

    instruction& sxth(const register_operand &dst, const register_operand &src) {
        return append(assembler::sxth(dst, src));
    }

    instruction& sxtw(const register_operand &dst, const register_operand &src) {
        return append(assembler::sxtw(dst, src));
    }

    instruction& uxtb(const register_operand &dst, const register_operand &src) {
        return append(assembler::uxtb(dst, src));
    }

    instruction& uxth(const register_operand &dst, const register_operand &src) {
        return append(assembler::uxth(dst, src));
    }

    instruction& uxtw(const register_operand &dst, const register_operand &src) {
        return append(assembler::uxtw(dst, src));
    }

    instruction& cas(const register_operand &dst, const register_operand &src,
                     const memory_operand &mem_addr) {
        return append(assembler::cas(dst, src, mem_addr).as_keep());
    }

    enum class atomic_types : std::uint8_t {
        exclusive,
        acquire,
        release,
    };

    instruction& atomic_load(const register_operand& dst, const memory_operand& mem,
                             atomic_types type = atomic_types::exclusive) {
        if (type == atomic_types::exclusive) {
            switch (dst.type().element_width()) {
            case 8:
                return append(assembler::ldxrb(dst, mem));
            case 16:
                return append(assembler::ldxrh(dst, mem));
            case 32:
            case 64:
                return append(assembler::ldxr(dst, mem));
            default:
                throw backend_exception("Cannot load atomically type {}", dst.type());
            }
        }

        switch (dst.type().element_width()) {
        case 8:
            return append(assembler::ldaxrb(dst, mem));
        case 16:
            return append(assembler::ldaxrh(dst, mem));
        case 32:
        case 64:
            return append(assembler::ldaxr(dst, mem));
        default:
            throw backend_exception("Cannot load atomically type {}", dst.type());
        }
    }

    instruction& atomic_store(const register_operand& status, const register_operand& rt,
                              const memory_operand& mem, atomic_types type = atomic_types::exclusive) {
        if (type == atomic_types::exclusive) {
            switch (status.type().element_width()) {
            case 8:
                return append(assembler::stxrb(status, rt, mem));
            case 16:
                return append(assembler::stxrh(status, rt, mem));
            case 32:
            case 64:
                return append(assembler::stxr(status, rt, mem));
            default:
                throw backend_exception("Cannot store atomically type {}", status.type());
            }
        }

        switch (status.type().element_width()) {
        case 8:
            return append(assembler::stlxrb(status, rt, mem));
        case 16:
            return append(assembler::stlxrh(status, rt, mem));
        case 32:
        case 64:
            return append(assembler::stlxr(status, rt, mem));
        default:
            throw backend_exception("Cannot store atomically type {}", status.type());
        }
    }

    void atomic_block(const register_operand &rm, const register_operand &rt,
                      const memory_operand &mem,
                      std::function<void()> body,
                      atomic_types type = atomic_types::exclusive)
    {
        const register_operand& status = vreg_alloc_.allocate(ir::value_type::u32());
        auto loop_label = fmt::format("loop_{}", instructions_.size());
        auto success_label = fmt::format("success_{}", instructions_.size());
        label(loop_label);
        atomic_load(rm, mem);

        body();

        atomic_store(status, rm, mem).add_comment("store if not failure");
        cbz(status, success_label).add_comment("== 0 represents success storing");
        b(loop_label).add_comment("loop until failure or success");
        label(success_label);
        return;
    }

    void atomic_add(const register_operand &rm, const register_operand &rt,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {

        if constexpr (!supports_lse) {
            atomic_block(rm, rt, mem, [this, &rm, &rt]() {
                adds(rt, rt, rm);
            }, type);
        }

        if (type == atomic_types::exclusive) {
            switch (rm.type().element_width()) {
            case 8:
                append(assembler::ldaddb(rm, rt, mem).as_keep());
                return;
            case 16:
                append(assembler::ldaddh(rm, rt, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldadd(rm, rt, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically add type {}", rt.type());
            }
        }

        // TODO
    }

    void atomic_sub(const register_operand &rm, const register_operand &rt,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        const auto& negated = vreg_alloc_.allocate(rt.type());
        neg(negated, rt);
        atomic_add(rm, negated, mem, type);
    }

    void atomic_xadd(const register_operand &rm, const register_operand &rt,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        const register_operand &old_dest = vreg_alloc_.allocate(rm.type());
        mov(old_dest, rm);
        atomic_add(rm, rt, mem, type);
        mov(rt, old_dest);
    }

    void atomic_clr(const register_operand &rm, const register_operand &rt,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            atomic_block(rm, rt, mem, [this, &rm, &rt]() {
                adds(rt, rt, rm);
            }, type);
        }

        if (type == atomic_types::exclusive) {
            switch (rm.type().element_width()) {
            case 8:
                append(assembler::ldclrb(rm, rt, mem).as_keep());
                return;
            case 16:
                append(assembler::ldclrh(rm, rt, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldclr(rm, rt, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", rt.type());
            }
        }

        // TODO
    }

    void atomic_and(const register_operand &rm, const register_operand &rt,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        not_(rt, rt);
        atomic_clr(rm, rt, mem, type);
    }

    void atomic_eor(const register_operand &rm, const register_operand &rt,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            atomic_block(rm, rt, mem, [this, &rm, &rt]() {
                eor_(rt, rt, rm);
            }, type);
        }
        if (type == atomic_types::exclusive) {
            switch (rm.type().element_width()) {
            case 8:
                append(assembler::ldeorb(rm, rt, mem).as_keep());
                return;
            case 16:
                append(assembler::ldeorh(rm, rt, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldeor(rm, rt, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", rt.type());
            }
        }

        // TODO
    }

    void atomic_set(const register_operand &rm, const register_operand &rt,
                    const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            atomic_block(rm, rt, mem, [this, &rm, &rt]() {
                orr_(rt, rt, rm);
            }, type);
        }
        if (type == atomic_types::exclusive) {
            switch (rm.type().element_width()) {
            case 8:
                append(assembler::ldsetb(rm, rt, mem).as_keep());
                return;
            case 16:
                append(assembler::ldseth(rm, rt, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldset(rm, rt, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", rt.type());
            }
        }

        // TODO
    }

    void atomic_smax(const register_operand &rm, const register_operand &rt,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            throw backend_exception("smax not implemented");
        }
        if (type == atomic_types::exclusive) {
            switch (rm.type().element_width()) {
            case 8:
                append(assembler::ldsmaxb(rm, rt, mem).as_keep());
                return;
            case 16:
                append(assembler::ldsmaxh(rm, rt, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldsmax(rm, rt, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", rt.type());
            }
        }

        // TODO
    }

    void atomic_smin(const register_operand &rm, const register_operand &rt,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            throw backend_exception("smin not implemented");
        }
        if (type == atomic_types::exclusive) {
            switch (rm.type().element_width()) {
            case 8:
                append(assembler::ldsminb(rm, rt, mem).as_keep());
                return;
            case 16:
                append(assembler::ldsminh(rm, rt, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldsmin(rm, rt, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", rt.type());
            }
        }

        // TODO
    }

    void atomic_umax(const register_operand &rm, const register_operand &rt,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            throw backend_exception("umax not implemented");
        }

        if (type == atomic_types::exclusive) {
            switch (rm.type().element_width()) {
            case 8:
                append(assembler::ldumaxb(rm, rt, mem).as_keep());
                return;
            case 16:
                append(assembler::ldumaxh(rm, rt, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldumax(rm, rt, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", rt.type());
            }
        }

        // TODO
    }


    void atomic_umin(const register_operand &rm, const register_operand &rt,
                            const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            throw backend_exception("umin not implemented");
            // atomic_block(rm, rt, mem, [this, &rm, &rt]() {
            //     adds(rt, rt, rm);
            // }, type);
        }
        if (type == atomic_types::exclusive) {
            switch (rm.type().element_width()) {
            case 8:
                append(assembler::lduminb(rm, rt, mem).as_keep());
                return;
            case 16:
                append(assembler::lduminh(rm, rt, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::ldumin(rm, rt, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", rt.type());
            }
        }

        // TODO
    }

    void atomic_swap(const register_operand &rm, const register_operand &rt,
                     const memory_operand &mem, atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            atomic_block(rm, rt, mem, [this, &rm, &rt]() {
                mov(rm, rt);
            }, type);
        }

        if (type == atomic_types::exclusive) {
            switch (rm.type().element_width()) {
            case 8:
                append(assembler::swpb(rm, rt, mem).as_keep());
                return;
            case 16:
                append(assembler::swph(rm, rt, mem).as_keep());
                return;
            case 32:
            case 64:
                append(assembler::swp(rm, rt, mem).as_keep());
                return;
            default:
                throw backend_exception("Cannot atomically clear type {}", rt.type());
            }
        }

        // TODO
    }

    void atomic_cmpxchg(const register_operand& current, const register_operand &acc,
                        const register_operand &src, const memory_operand &mem,
                        atomic_types type = atomic_types::exclusive)
    {
        if constexpr (!supports_lse) {
            cmp(current, acc).add_comment("compare with accumulator");
            csel(acc, current, acc, cond_operand::ne()).add_comment("conditionally move current memory value into accumulator");
            return;
        }

        insert_comment("Atomic CMPXCHG using CAS (enabled on systems with LSE support");
        cas(acc, src, mem).add_comment("write source to memory if source == accumulator, accumulator = source");
        cmp(acc, 0);
        mov(current, acc);

        // TODO
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
            // mov<immediates_strict_policy>(reg, immediate & mask);
            // TODO: fix
            mov(reg, immediate & mask);
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
    constexpr static bool supports_lse = false;
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

} // namespace arancini::output::dynamic::arm64

