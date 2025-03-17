#pragma once

#include <arancini/ir/port.h>
#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/arm64/arm64-assembler.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>

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

    void bound_to_type(const register_operand& var, ir::value_type type) {
        // if (is_bignum(var.type()) || type.is_vector())
        //     throw backend_exception("Cannot bound big scalar of type {} to type {}", var.type(), type);

        auto mask_immediate = 1 << (var.type().width() - 1);
        auto mask = move_immediate(mask_immediate, var.type());
        append(arm64_assembler::and_(var, var, mask));
    }

	instruction& add(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
		return append(arm64_assembler::add(dst, src1, src2));
    }

    instruction& add(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2,
                      const shift_operand &shift) {
        return append(arm64_assembler::add(dst, src1, src2, shift));
    }

	instruction& adds(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        return append(arm64_assembler::adds(dst, src1, src2));
    }

    instruction& adds(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2,
                      const shift_operand &shift) {
        return append(arm64_assembler::adds(dst, src1, src2, shift));
    }

	instruction& adcs(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        return append(arm64_assembler::adcs(dst, src1, src2));
    }

	instruction& sub(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        return append(arm64_assembler::sub(dst, src1, src2));
    }

    instruction& sub(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2,
                      const shift_operand &shift) {
        return append(arm64_assembler::sub(dst, src1, src2, shift));
    }

	instruction& subs(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        return append(arm64_assembler::subs(dst, src1, src2));
    }

    instruction& subs(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2,
                      const shift_operand &shift) {
        return append(arm64_assembler::subs(dst, src1, src2, shift));
    }

	instruction& sbc(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        return append(arm64_assembler::sbc(dst, src1, src2));
    }

	instruction& sbcs(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        return append(arm64_assembler::sbcs(dst, src1, src2));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& orr_(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        // TODO: this checks that the immediate is between immr:imms (bits [21:10])
        // However, for 64-bit orr, we have N:immr:imms, which gives us bits [22:10])
        ImmediatesPolicy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        return append(arm64_assembler::orr(dst, src1, policy(src2)));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& and_(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        // TODO: this checks that the immediate is between immr:imms (bits [21:10])
        // However, for 64-bit and, we have N:immr:imms, which gives us bits [22:10])
        ImmediatesPolicy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        return append(arm64_assembler::and_(dst, src1, policy(src2)));
    }

    // TODO: refactor this; there should be only a single version and the comment should be removed
    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& ands(const register_operand &dst,
                      const register_operand &src1,
                      const reg_or_imm &src2) {
        // TODO: this checks that the immediate is between immr:imms (bits [21:10])
        // However, for 64-bit ands, we have N:immr:imms, which gives us bits [22:10])
        ImmediatesPolicy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        return append(arm64_assembler::ands(dst, src1, policy(src2)));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& eor_(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2) {
        // TODO: this checks that the immediate is between immr:imms (bits [21:10])
        // However, for 64-bit eor, we have N:immr:imms, which gives us bits [22:10])
        ImmediatesPolicy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        return append(arm64_assembler::eor(dst, src1, policy(src2)));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& not_(const register_operand &dst, const reg_or_imm &src) {
        ImmediatesPolicy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 6), dst.type());
        return append(arm64_assembler::mvn(dst, policy(src)));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& neg(const register_operand &dst, const register_operand &src) {
        ImmediatesPolicy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 6), dst.type());
        return append(arm64_assembler::neg(dst, policy(src)));
    }

    instruction& movn(const register_operand &dst,
              const immediate_operand &src,
              const shift_operand &shift) {
        return append(arm64_assembler::movn(dst, src, shift));
    }

    instruction& movz(const register_operand &dst,
              const immediate_operand &src,
              const shift_operand &shift) {
        return append(arm64_assembler::movz(dst, src, shift));
    }

    instruction& movk(const register_operand &dst,
              const immediate_operand &src,
              const shift_operand &shift) {
        return append(arm64_assembler::movk(dst, src, shift));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& mov(const register_operand &dst, const reg_or_imm &src) {
        ImmediatesPolicy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        return append(arm64_assembler::mov(dst, policy(src)));
    }

    void load(const register_sequence& out, const memory_operand& address) {
        for (std::size_t i = 0; i < out.size(); ++i) {
            if (out[i].type().element_width() <= 8) {
                memory_operand memory(address.base_register(), address.offset().value() + i);
                append(arm64_assembler::ldrb(out[i], memory));
            } else if (out[i].type().element_width() <= 16) {
                memory_operand memory(address.base_register(), address.offset().value() + i * 2);
                append(arm64_assembler::ldrh(out[i], memory));
            } else {
                auto offset = i * (out[i].type().element_width() < 64 ? 4 : 8);
                memory_operand memory(address.base_register(), address.offset().value() + offset);
                append(arm64_assembler::ldr(out[i], memory));
            }
        }
    }

    void store(const register_sequence& source, const memory_operand& address) {
        for (std::size_t i = 0; i < source.size(); ++i) {
            if (source[i].type().element_width() <= 8) {
                memory_operand memory(address.base_register(), address.offset().value() + i);
                append(arm64_assembler::strb(source[i], memory));
            } else if (source[i].type().element_width() <= 16) {
                memory_operand memory(address.base_register(), address.offset().value() + i * 2);
                append(arm64_assembler::strh(source[i], memory));
            } else {
                auto offset = i * (source[i].type().element_width() < 64 ? 4 : 8);
                memory_operand memory(address.base_register(), address.offset().value() + offset);
                append(arm64_assembler::str(source[i], memory));
            }
        }
    }

    instruction& branch(label_operand &dest) {
        label_refcount_[dest.name()]++;
		return append(arm64_assembler::b(dest));
    }

    instruction& branch(label_operand &dest, const cond_operand& cond) {
        label_refcount_[dest.name()]++;

        if (cond.condition() == "eq")
            return append(arm64_assembler::beq(dest));

        if (cond.condition() == "ne")
            return append(arm64_assembler::bne(dest));
        
        [[likely]]
        if (cond.condition() == "lt")
            return append(arm64_assembler::bl(dest));

        throw backend_exception("Cannot branch with condition {}", cond);
    }

    // Check reg == 0 and jump if true
    // Otherwise, continue to the next instruction
    // Does not affect condition flags (can be used to compare-and-branch with 1 instruction)
    instruction& zero_compare_and_branch(const register_operand &reg, 
                                         const label_operand &label,
                                         const cond_operand& cond) 
    {
        label_refcount_[label.name()]++;

        if (cond.condition() == "eq")
            return append(arm64_assembler::cbz(reg, label));

        [[likely]]
        if (cond.condition() == "ne")
            return append(arm64_assembler::cbnz(reg, label));

        throw backend_exception("Cannot zero-compare-and-branch on condition {}", cond);
    }

    // TODO: handle register_set
    instruction& cmn(const register_operand &dst,
                     const reg_or_imm &src) {
        return append(arm64_assembler::cmn(dst, src));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& cmp(const register_operand &dst, const reg_or_imm &src) {
        ImmediatesPolicy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), dst.type());
        return append(arm64_assembler::cmp(dst, policy(src)));
    }

    instruction& tst(const register_operand &dst, const reg_or_imm &src) {
        return append(arm64_assembler::tst(dst, src));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& lsl(const register_operand &dst, const register_operand &src1, const reg_or_imm &src2) {
        std::size_t size = dst.type().element_width() <= 32 ? 4 : 6;
        immediates_upgrade_policy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), dst.type());
        return append(arm64_assembler::lsl(dst, src1, policy(src2)));
    }

    template <typename ImmediatesPolicy = immediates_upgrade_policy>
    instruction& lsr(const register_operand &dst,
                     const register_operand &src1,
                     const reg_or_imm &src2) {
        std::size_t size = dst.type().element_width() <= 32 ? 5 : 6;
        immediates_upgrade_policy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), dst.type());
        return append(arm64_assembler::lsr(dst, src1, policy(src2)));
    }

    instruction& asr(const register_operand &dst,
                     const register_operand &src1,
                     const reg_or_imm &src2) {
        std::size_t size = dst.type().element_width() <= 32 ? 5 : 6;
        immediates_upgrade_policy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), dst.type());
        return append(arm64_assembler::asr(dst, src1, policy(src2)));
    }

    instruction& extr(const register_operand &dst,
                      const register_operand &src1,
                      const register_operand &src2,
                      const immediate_operand &shift) 
    {
        return append(arm64_assembler::extr(dst, src1, src2, shift));
    }

    instruction& csel(const register_operand &dst,
                      const register_operand &src1,
                      const register_operand &src2,
                      const cond_operand &cond) {
        return append(arm64_assembler::csel(dst, src1, src2, cond));
    }

    instruction& cset(const register_operand &dst, const cond_operand &cond) {
        return append(arm64_assembler::cset(dst, cond));
    }

    instruction& bfxil(const register_operand &dst,
                       const register_operand &src1,
                       const immediate_operand &lsb,
                       const immediate_operand &width)
    {
        return append(arm64_assembler::bfxil(dst, src1, lsb, width));
    }

    instruction& ubfx(const register_operand &dst,
                      const register_operand &src1,
                      const immediate_operand &lsb,
                      const immediate_operand &width)
    {
        return append(arm64_assembler::ubfx(dst, src1, lsb, width));
    }

    instruction& bfi(const register_operand &dst,
                     const register_operand &src1,
                     const immediate_operand &lsb,
                     const immediate_operand &width)
    {
        return append(arm64_assembler::bfi(dst, src1, lsb, width));
    }

    instruction& mul(const register_operand &dest,
             const register_operand &src1,
             const register_operand &src2) {
        return append(arm64_assembler::mul(dest, src1, src2));
    }

    instruction& smulh(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(arm64_assembler::smulh(dest, src1, src2));
    }

    instruction& smull(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(arm64_assembler::smull(dest, src1, src2));
    }

    instruction& umulh(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(arm64_assembler::umulh(dest, src1, src2));
    }

    instruction& umull(const register_operand &dest,
               const register_operand &src1,
               const register_operand &src2) {
        return append(arm64_assembler::umull(dest, src1, src2));
    }

    instruction& sdiv(const register_operand &dest,
              const register_operand &src1,
              const register_operand &src2) {
        return append(arm64_assembler::sdiv(dest, src1, src2));
    }

    instruction& udiv(const register_operand &dest,
              const register_operand &src1,
              const register_operand &src2) {
        return append(arm64_assembler::udiv(dest, src1, src2));
    }

    instruction& fmul(const register_operand &dest,
                      const register_operand &src1,
                      const register_operand &src2) {
        return append(arm64_assembler::fmul(dest, src1, src2));
    }

    instruction& fdiv(const register_operand &dest,
                      const register_operand &src1,
                      const register_operand &src2) {
        return append(arm64_assembler::fdiv(dest, src1, src2));
    }

    instruction& fmov(const register_operand &dest,
                      const register_operand &src) {
        return append(arm64_assembler::fmov(dest, src));
    }

    instruction& fmov(const register_operand &dest,
                      const immediate_operand &src) {
        return append(arm64_assembler::fmov(dest, src));
    }

    instruction& fcvt(const register_operand &dest,
                      const register_operand &src) 
    {
        return append(arm64_assembler::fcvt(dest, src));
    }

    instruction& fcvtzs(const register_operand &dest, const register_operand &src) {
        return append(arm64_assembler::fcvtzs(dest, src));
    }

    instruction& fcvtzu(const register_operand &dest, const register_operand &src) {
        return append(arm64_assembler::fcvtzu(dest, src));
    }

    instruction& fcvtas(const register_operand &dest, const register_operand &src) {
        return append(arm64_assembler::fcvtas(dest, src));
    }

    instruction& fcvtau(const register_operand &dest, const register_operand &src) {
        return append(arm64_assembler::fcvtau(dest, src));
    }

    // TODO: this also has an immediate variant
    instruction& fcmp(const register_operand &dest, const register_operand &src) {
        return append(arm64_assembler::fcmp(dest, src));
    }

    instruction& scvtf(const register_operand &dest, const register_operand &src) {
        return append(arm64_assembler::scvtf(dest, src));
    }

    instruction& ucvtf(const register_operand &dest, const register_operand &src) {
        return append(arm64_assembler::ucvtf(dest, src));
    }

    instruction& mrs(const register_operand &dest,
             const register_operand &src) {
        return append(arm64_assembler::mrs(dest, src));
    }

    instruction& msr(const register_operand &dest,
                     const register_operand &src) {
        return append(arm64_assembler::msr(dest, src));
    }

    instruction& ret() {
        return append(arm64_assembler::ret());
    }

    instruction& brk(const immediate_operand &imm) {
        return append(arm64_assembler::brk(imm));
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
        return append(arm64_assembler::cset(dst, cond_operand::eq())
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

	instruction& sets(const register_operand &dst) {
        return append(arm64_assembler::cset(dst, cond_operand::lt())
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

	instruction& setc(const register_operand &dst) {
        return append(arm64_assembler::cset(dst, cond_operand::cs())
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

	instruction& setcc(const register_operand &dst) {
        return append(arm64_assembler::cset(dst, cond_operand::cc())
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }
	instruction& seto(const register_operand &dst) {
        return append(arm64_assembler::cset(dst, cond_operand::vs())
                      .implicitly_reads({register_operand(register_operand::nzcv)}));
    }

    instruction& cfinv() {
        return append(instruction("cfinv"));
    }

    void zero_extend(const register_operand& out, const register_operand& source) {
        if (out.type().width() == source.type().width()) {
            append(arm64_assembler::mov(out, source));
            return;
        }
    
        // Sanity check
        // TODO: more missing sanity checks
        [[unlikely]]
        if (out.type().width() < source.type().width())
            throw backend_exception("Cannot zero-extend {} to smaller size {}",
                                    source.type(), out.type());
    
        insert_comment("zero-extend from {} to {}", source.type(), out.type());
        if (source.type().width() <= 8) {
            append(arm64_assembler::uxtb(out, source));
        } else if (source.type().width() <= 16) {
            append(arm64_assembler::uxth(out, source));
        } else if (source.type().width() <= 32) {
            append(arm64_assembler::uxtw(out, source));
        } else {
            append(arm64_assembler::mov(out, source));
        }
    }

    void sign_extend(const register_operand& out, const register_operand& source) {
        if (out.type().width() == source.type().width()) {
            append(arm64_assembler::mov(out, source));
            return;
        }
    
        // Sanity check
        // TODO: more missing sanity checks
        [[unlikely]]
        if (out.type().width() < source.type().width())
            throw backend_exception("Cannot zero-extend {} to smaller size {}",
                                    source.type(), out.type());
    
        insert_comment("zero-extend from {} to {}", source.type(), out.type());
        if (source.type().width() < 8) {
            append(arm64_assembler::sxtb(out, source));
            bound_to_type(out, source.type());
        } else if (source.type().width() == 8) {
            append(arm64_assembler::sxtb(out, source));
        } else if (source.type().width() <= 16) {
            append(arm64_assembler::sxth(out, source));
        } else if (source.type().width() <= 32) {
            append(arm64_assembler::sxtw(out, source));
        } else {
            append(arm64_assembler::mov(out, source));
        }
    }

    void extend(const register_operand& out, const register_operand& source) {
        if (source.type().is_signed())
            return sign_extend(out, source);
        return zero_extend(out, source);
    }

    instruction& cas(const register_operand &dst, const register_operand &src,
                     const memory_operand &mem_addr) {
        return append(instruction("cas", use(dst), use(src), use(mem_addr)).as_keep());
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

	instruction& append(const instruction &i) {
        instructions_.push_back(i);
        return instructions_.back();
    }
private:
    arm64_assembler asm_;
	std::vector<instruction> instructions_;
    virtual_register_allocator vreg_alloc_;
    std::unordered_map<std::string, std::size_t> label_refcount_;
    std::unordered_set<std::string> labels_;

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
            builder_->append(arm64_assembler::ldxrb(data_reg, mem_addr_)).add_comment("load atomically");
            break;
        case 16:
            builder_->append(arm64_assembler::ldxrh(data_reg, mem_addr_)).add_comment("load atomically");
            break;
        case 32:
        case 64:
            builder_->append(arm64_assembler::ldxr(data_reg, mem_addr_)).add_comment("load atomically");
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
            builder_->append(arm64_assembler::stxrb(status_reg, src_reg, mem_addr_)).add_comment("store if not failure");
            break;
        case 16:
            builder_->append(arm64_assembler::stxrh(status_reg, src_reg, mem_addr_)).add_comment("store if not failure");
            break;
        case 32:
        case 64:
            builder_->append(arm64_assembler::stxr(status_reg, src_reg, mem_addr_)).add_comment("store if not failure");
            break;
        default:
            throw backend_exception("Cannot store atomically values of type {}",
                                    src_reg.type());
        }
        // Compare and also set flags for later
        builder_->zero_compare_and_branch(status_reg, success_label_, cond_operand::eq())
                .add_comment("== 0 represents success storing");
        builder_->branch(loop_label_).add_comment("loop until failure or success");
        builder_->label(success_label_);
    }

    memory_operand mem_addr_;
    label_operand loop_label_;
    label_operand success_label_;
    instruction_builder* builder_;
};

} // namespace arancini::output::dynamic::arm64

