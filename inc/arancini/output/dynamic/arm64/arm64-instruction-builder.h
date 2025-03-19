#pragma once

#include <arancini/ir/port.h>
#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/arm64/arm64-assembler.h>
#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <vector>
#include <atomic>
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

    operator const register_operand&() const {
        [[unlikely]]
        if (regs_.size() > 1)
            throw backend_exception("Accessing register set of {} registers as single register",
                                    regs_.size());
        return regs_[0];
    }

    operator const std::vector<register_operand>&() const {
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

    ir::value_type type() const {
        if (regs_.size() == 1)
            return regs_[0].type();
            
        // Big scalars
        return ir::value_type(regs_[0].type().type_class(),
                              regs_[0].type().width() * regs_.size());
    }

    using iterator = std::vector<register_operand>::iterator;
    using const_iterator = std::vector<register_operand>::const_iterator;

    iterator begin() { return regs_.begin(); }
    const_iterator begin() const { return regs_.begin(); }
    const_iterator cbegin() const { return regs_.cbegin(); }

    iterator end() { return regs_.end(); }
    const_iterator end() const { return regs_.end(); }
    const_iterator cend() const { return regs_.cend(); }
private:
    std::vector<register_operand> regs_;
};

class variable {
    std::vector<register_sequence> values_;
public:
    variable(const register_operand& reg):
        values_({{reg}})
    { } 

    variable(const register_sequence& regseq):
        values_({regseq})
    {
        [[unlikely]]
        if (regseq.type().is_vector())
            throw backend_exception("Attempting to create variable from vector register sequence");

        // TODO: may prefer to relax this constraint
        [[unlikely]]
        if (!regseq.size())
            throw backend_exception("Attempting to create variable from empty register sequence");
    } 

    variable(const std::vector<register_sequence>& emulated_vector):
        values_(emulated_vector)
    { 
        [[unlikely]]
        if (emulated_vector.size() && emulated_vector[0].type().is_vector())
            throw backend_exception("Attempting to create variable from vector register sequence");
    } 

    operator const register_operand&() const {
        return values_[0];
    }

    register_sequence& operator[](std::size_t idx) {
        return values_[idx];
    }

    const register_sequence& operator[](std::size_t idx) const {
        return values_[idx];
    }

    register_sequence& scalar() {
        if (values_.size() != 1)
            throw backend_exception("Cannot treat vector type {} as scalar", type());
        return values_[0];
    }

    const register_sequence& scalar() const {
        if (values_.size() != 1)
            throw backend_exception("Cannot treat vector type {} as scalar", type());
        return values_[0];
    }

    bool is_native_vector() const {
        return type().is_vector() && values_.size() == 1;
    }

    std::size_t size() const { 
        if (values_.size() == 1)
            return values_[0].size();
        return values_.size();
    }

    ir::value_type type() const {
        if (values_.size() == 1) {
            // For normal scalars, real vectors and big scalars
            return values_[0].type();
        }
        
        // For emulated vectors
        return ir::value_type::vector(values_[0].type(), values_.size());
    }

    using iterator = std::vector<register_sequence>::iterator;
    using const_iterator = std::vector<register_sequence>::const_iterator;

    iterator begin() { return values_.begin(); }
    const_iterator begin() const { return values_.begin(); }
    const_iterator cbegin() const { return values_.cbegin(); }

    iterator end() { return values_.end(); }
    const_iterator end() const { return values_.end(); }
    const_iterator cend() const { return values_.cend(); }
};

class virtual_register_allocator final {
public:
    [[nodiscard]]
    register_sequence allocate([[maybe_unused]] ir::value_type type);

    [[nodiscard]]
    variable allocate_variable(ir::value_type type);

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

    void inverse(const variable& out, const variable& source) {
        // TODO: compare types without signs
        if (out.type().is_vector()) {
            if (out.is_native_vector())
                throw backend_exception("Cannot inverse native vectors");

            for (auto out_it = out.begin(), src_it = source.begin(); out_it != out.end(); ++out_it, ++src_it)
                inverse(*out_it, *src_it);
        }

        return inverse(out.scalar(), source.scalar());
    }

    void negate(const variable& out, const variable& source) {
        // TODO: compare types without signs
        if (out.type().is_vector()) {
            if (out.is_native_vector())
                throw backend_exception("Cannot negate native vectors");

            for (auto out_it = out.begin(), src_it = source.begin(); out_it != out.end(); ++out_it, ++src_it)
                negate(*out_it, *src_it);
        }

        return negate(out.scalar(), source.scalar());
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

    void load(const variable& out, const memory_operand& address) {
        if (out.type().is_vector()) {
            if (out.is_native_vector())
                throw backend_exception("Cannot load native vectors");

            for (const auto& vec_elem : out)
                return load(vec_elem, address);
        }

        return load(out.scalar(), address);
    }

    void store(const variable& source, const memory_operand& address) {
        if (source.type().is_vector()) {
            if (source.is_native_vector())
                throw backend_exception("Cannot store native vectors");

            for (const auto& vec_elem : source)
                return store(vec_elem, address);
        }

        return store(source.scalar(), address);
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

    void compare(const variable& source1, const variable& source2) {
        // TODO: check types
        if (source1.type().is_vector())
            throw backend_exception("Cannot compare vectors");

        append(arm64_assembler::cmp(source1, source2));
    }

    void compare(const variable& source1, const immediate_operand& source2) {
        // TODO: check types
        if (source1.type().is_vector())
            throw backend_exception("Cannot compare vectors");

        immediates_upgrade_policy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, 12), source1.type());
        append(arm64_assembler::cmp(source1, policy(source2)));
    }

    void shift_left(const variable& out, const variable& input, const variable& amount) {
        if (out.type().is_vector())
            throw backend_exception("Cannot shift left vector of type {}", out.type());

        append(arm64_assembler::lsl(out, input, amount));
    }

    void shift_left(const variable& out, const variable& input, immediate_operand amount) {
        if (out.type().is_vector())
            throw backend_exception("Cannot shift left vector of type {}", out.type());

        std::size_t size = out.type().element_width() <= 32 ? 5 : 6;
        immediates_upgrade_policy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), out.type());
        append(arm64_assembler::lsl(out, input, policy(amount)));
    }

    void logical_shift_right(const variable& out, const variable& input, const variable& amount) {
        if (out.type().is_vector())
            throw backend_exception("Cannot logically shift right vector of type {}", out.type());

        append(arm64_assembler::lsr(out, input, amount));
    }

    void logical_shift_right(const variable& out, const variable& input, immediate_operand amount) {
        if (out.type().is_vector())
            throw backend_exception("Cannot logically shift right vector of type {}", out.type());


        if (out.type().width() == 128) {
            if (amount.value() < 64) {
                append(arm64_assembler::extr(out.scalar()[0], input.scalar()[1], input.scalar()[0], amount));
                append(arm64_assembler::lsr(out.scalar()[1], input.scalar()[1], amount));
                return;
            } else if (amount.value() == 64) {
                append(arm64_assembler::mov(out.scalar()[0], input.scalar()[1]));
                append(arm64_assembler::mov(out.scalar()[1], 0));
                return;
            }

            throw backend_exception("Unsupported logical right-shift operation with amount {}", amount);
        } 

        std::size_t size = out.type().element_width() <= 32 ? 5 : 6;
        immediates_upgrade_policy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), out.type());
        append(arm64_assembler::lsr(out, input, policy(amount)));
    }

    void arithmetic_shift_right(const variable& out, const variable& input, const variable& amount) {
        if (out.type().is_vector())
            throw backend_exception("Cannot arithmetically shift right vector of type {}", out.type());

        append(arm64_assembler::asr(out, input, amount));
    }

    void arithmetic_shift_right(const variable& out, const variable& input, immediate_operand amount) {
        if (out.type().is_vector())
            throw backend_exception("Cannot arithmetically shift right vector of type {}", out.type());

        std::size_t size = out.type().element_width() <= 32 ? 5 : 6;
        immediates_upgrade_policy policy(this, ir::value_type(ir::value_type_class::unsigned_integer, size), out.type());
        append(arm64_assembler::asr(out, input, policy(amount)));
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

    void atomic_load(const register_operand& out, const memory_operand& address, 
                     std::memory_order mem_order = std::memory_order_acquire) {
        if (mem_order != std::memory_order_acquire && mem_order != std::memory_order_relaxed)
            throw backend_exception("Memory order {} not supported for atomic load (only acquire and relaxed supported)",
                                    util::to_underlying(mem_order));

        [[unlikely]]
        if (mem_order == std::memory_order_relaxed) {
            if (out.type().width() <= 8) {
                append(arm64_assembler::ldxrb(out, address));
                return;
            } else if (out.type().width() <= 16) {
                append(arm64_assembler::ldxrh(out, address));
                return;
            }
    
            append(arm64_assembler::ldxr(out, address));
            return;
        }
    
        // Acquire
        switch (out.type().element_width()) {
        case 8:
            append(arm64_assembler::ldaxrb(out, address));
            return;
        case 16:
            append(arm64_assembler::ldaxrh(out, address));
            return;
        case 32:
        case 64:
            append(arm64_assembler::ldaxr(out, address));
            return;
        default:
            throw backend_exception("Cannot load atomically type {}", out.type());
        }
    }

    void atomic_store(const register_operand& status, const register_operand& source,
                      const memory_operand& address, 
                      std::memory_order mem_order = std::memory_order_release) {

        if (mem_order != std::memory_order_release && mem_order != std::memory_order_relaxed)
            throw backend_exception("Memory order {} not supported for atomic store (only release and relaxed supported)",
                                    util::to_underlying(mem_order));

        [[unlikely]]
        if (mem_order == std::memory_order_relaxed) {
            if (source.type().width() <= 8) {
                append(arm64_assembler::stxrb(status, source, address));
                return;
            } else if (source.type().width() <= 16) {
                append(arm64_assembler::stxrh(status, source, address));
                return;
            }
    
            append(arm64_assembler::stxr(status, source, address));
            return;
        }
    
        // Release
        switch (source.type().element_width()) {
        case 8:
            append(arm64_assembler::stlxrb(status, source, address));
            return;
        case 16:
            append(arm64_assembler::stlxrh(status, source, address));
            return;
        case 32:
        case 64:
            append(arm64_assembler::stlxr(status, source, address));
            return;
        default:
            throw backend_exception("Cannot store atomically type {}", source.type());
        }
    }

    void atomic_block(const register_operand &data, const memory_operand &mem,
                      std::function<void()> atomic_logic, 
                      std::memory_order load_mem_order = std::memory_order_acquire,
                      std::memory_order store_mem_order = std::memory_order_release) {
        auto status = vreg_alloc_.allocate(ir::value_type::u32());
        auto loop_label = label_operand(fmt::format("loop_{}", instructions_.size()));
        auto success_label = label_operand(fmt::format("success_{}", instructions_.size()));
        label(loop_label);
        atomic_load(data, mem, load_mem_order);

        atomic_logic();

        atomic_store(status, data, mem, store_mem_order);
        zero_compare_and_branch(status, success_label, cond_operand::eq());
        branch(loop_label);
        label(success_label);
        return;
    }

    static constexpr auto default_memory_order = std::memory_order_acq_rel;
    void atomic_add(const register_operand& out, const register_operand& source,
                    const memory_operand& address,
                    std::memory_order mem_order = default_memory_order)
    {
        if (!asm_.supports_lse()) {
            auto [load_mem_order, store_mem_order] = determine_memory_order(mem_order);
            atomic_block(out, source, [this, &out, &source]() {
                adds(out, source, out);
            }, load_mem_order, store_mem_order);
            return;
        }

        switch (out.type().element_width()) {
        case 8:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::ldaddb(out, source, address));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::ldaddab(out, source, address));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::ldaddalb(out, source, address));
                break;
            default:
                throw backend_exception("Cannot atomically add byte for memory order {}", util::to_underlying(mem_order));
            }
            return;
        case 16:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::ldaddh(out, source, address));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::ldaddah(out, source, address));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::ldaddalh(out, source, address));
                break;
            default:
                throw backend_exception("Cannot atomically add halfword for memory order {}", util::to_underlying(mem_order));
            }
            return;
        case 32:
        case 64:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::ldadd(out, source, address));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::ldadda(out, source, address));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::ldaddal(out, source, address));
                break;
            default:
                throw backend_exception("Cannot atomically add for memory order {}", util::to_underlying(mem_order));
            }
            return;
        default:
            throw backend_exception("Cannot atomically add type {}", source.type());
        }
    }

    void atomic_sub(const register_operand& out, const register_operand& source,
                    const memory_operand &address, 
                    std::memory_order mem_order = default_memory_order)
    {
        auto negated = vreg_alloc_.allocate(source.type());
        negate(negated, source);
        atomic_add(out, negated, address, mem_order);
    }

    void atomic_xadd(const register_operand& out, const register_operand& source,
                     const memory_operand& address,
                     std::memory_order mem_order = default_memory_order)
    {
        register_operand old = vreg_alloc_.allocate(out.type());
        append(arm64_assembler::mov(old, out));
        atomic_add(out, source, address, mem_order);
        append(arm64_assembler::mov(source, old));
    }

    void atomic_clr(const register_operand& out, const register_operand& source,
        const memory_operand &mem, std::memory_order mem_order = default_memory_order)
    {
        if (!asm_.supports_lse()) {
            auto [load_mem_order, store_mem_order] = determine_memory_order(mem_order);

            atomic_block(out, mem, [this, &out, &source]() {
                auto negated = vreg_alloc_.allocate(source.type());
                inverse(negated, source);
                ands(out, out, negated);
            }, load_mem_order, store_mem_order);
            return;
        }

        switch (out.type().element_width()) {
        case 8:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::ldclrb(out, source, mem));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::ldclrab(out, source, mem));
                break;
            case std::memory_order_release:
                append(arm64_assembler::ldclrlb(out, source, mem));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::ldclralb(out, source, mem));
                break;
            default:
                throw backend_exception("Cannot atomically clear byte for memory order {}", util::to_underlying(mem_order));
            }
            return;
        case 16:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::ldclrh(out, source, mem));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::ldclrah(out, source, mem));
                break;
            case std::memory_order_release:
                append(arm64_assembler::ldclrlh(out, source, mem));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::ldclralh(out, source, mem));
                break;
            default:
                throw backend_exception("Cannot atomically clear byte for memory order {}", util::to_underlying(mem_order));
            }
            return;
        case 32:
        case 64:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::ldclr(out, source, mem));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::ldclra(out, source, mem));
                break;
            case std::memory_order_release:
                append(arm64_assembler::ldclrl(out, source, mem));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::ldclral(out, source, mem));
                break;
            default:
                throw backend_exception("Cannot atomically clear byte for memory order {}", util::to_underlying(mem_order));
            }
            return;
        default:
            throw backend_exception("Cannot atomically clear type {}", source.type());
        }
    }

    void atomic_and(const register_operand& out, const register_operand& source,
            const memory_operand &mem, std::memory_order mem_order = default_memory_order)
    {
        auto complemented = vreg_alloc_.allocate(source.type());
        inverse(complemented, source);
        atomic_clr(out, complemented, mem, mem_order);
    }

    void atomic_xor(const register_operand& out, const register_operand& source,
            const register_operand& mem, std::memory_order mem_order = default_memory_order)
    {
        if (!asm_.supports_lse()) {
            auto [load_mem_order, store_mem_order] = determine_memory_order(mem_order);

            atomic_block(out, mem, [this, &out, &source]() {
                append(arm64_assembler::eor(out, out, source));
            }, load_mem_order, store_mem_order);
            return;
        }

        switch (out.type().element_width()) {
        case 8:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::ldeorb(out, source, mem));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::ldeorab(out, source, mem));
                break;
            case std::memory_order_release:
                append(arm64_assembler::ldeorlb(out, source, mem));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::ldeoralb(out, source, mem));
                break;
            default:
                throw backend_exception("Cannot atomically XOR byte for memory order {}", util::to_underlying(mem_order));
            }
            return;
        case 16:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::ldeorh(out, source, mem));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::ldeorah(out, source, mem));
                break;
            case std::memory_order_release:
                append(arm64_assembler::ldeorlh(out, source, mem));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::ldeoralh(out, source, mem));
                break;
            default:
                throw backend_exception("Cannot atomically XOR byte for memory order {}", util::to_underlying(mem_order));
            }
            return;
        case 32:
        case 64:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::ldeor(out, source, mem));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::ldeora(out, source, mem));
                break;
            case std::memory_order_release:
                append(arm64_assembler::ldeorl(out, source, mem));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::ldeoral(out, source, mem));
                break;
            default:
                throw backend_exception("Cannot atomically XOR byte for memory order {}", util::to_underlying(mem_order));
            }
            return;
        default:
            throw backend_exception("Cannot atomically XOR type {}", source.type());
        }
    }

    void atomic_or(const register_operand& out, const register_operand& source,
            const register_operand& mem, std::memory_order mem_order = default_memory_order)
    {
        if (!asm_.supports_lse()) {
            auto [load_mem_order, store_mem_order] = determine_memory_order(mem_order);

            atomic_block(out, mem, [this, &out, &source]() {
                append(arm64_assembler::orr(out, out, source));
            }, load_mem_order, store_mem_order);
            return;
        }

        switch (out.type().element_width()) {
        case 8:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::ldsetb(out, source, mem));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::ldsetab(out, source, mem));
                break;
            case std::memory_order_release:
                append(arm64_assembler::ldsetlb(out, source, mem));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::ldsetalb(out, source, mem));
                break;
            default:
                throw backend_exception("Cannot atomically OR byte for memory order {}", util::to_underlying(mem_order));
            }
            return;
        case 16:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::ldseth(out, source, mem));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::ldsetah(out, source, mem));
                break;
            case std::memory_order_release:
                append(arm64_assembler::ldsetlh(out, source, mem));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::ldsetalh(out, source, mem));
                break;
            default:
                throw backend_exception("Cannot atomically OR halfword for memory order {}", util::to_underlying(mem_order));
            }
            return;
        case 32:
        case 64:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::ldset(out, source, mem));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::ldseta(out, source, mem));
                break;
            case std::memory_order_release:
                append(arm64_assembler::ldsetl(out, source, mem));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::ldsetal(out, source, mem));
                break;
            default:
                throw backend_exception("Cannot atomically OR for memory order {}", util::to_underlying(mem_order));
            }
            return;
        default:
            throw backend_exception("Cannot atomically clear type {}", out.type());
        }
    }

    void atomic_swap(const register_operand& out, const register_operand& source,
                     const memory_operand &mem, 
                     std::memory_order mem_order = std::memory_order_acq_rel)
    {
        if (!asm_.supports_lse()) {
            auto [load_mem_order, store_mem_order] = determine_memory_order(mem_order);

            atomic_block(out, mem, [this, &out, &source]() {
                auto old = vreg_alloc_.allocate(out.type());
                append(arm64_assembler::mov(old, out));
                append(arm64_assembler::mov(out, source));
                append(arm64_assembler::mov(source, old));
            }, load_mem_order, store_mem_order);
            return;
        }

        switch (out.type().element_width()) {
        case 8:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::swpb(out, source, mem));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::swpab(out, source, mem));
                break;
            case std::memory_order_release:
                append(arm64_assembler::swplb(out, source, mem));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::swpalb(out, source, mem));
                break;
            default:
                throw backend_exception("Cannot atomically swap byte for memory order {}", util::to_underlying(mem_order));
            }
            return;
        case 16:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::swph(out, source, mem));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::swpah(out, source, mem));
                break;
            case std::memory_order_release:
                append(arm64_assembler::swplh(out, source, mem));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::swpalh(out, source, mem));
                break;
            default:
                throw backend_exception("Cannot atomically swap halfword for memory order {}", util::to_underlying(mem_order));
            }
            return;
        case 32:
        case 64:
            switch (mem_order) {
            case std::memory_order_relaxed:
                append(arm64_assembler::swp(out, source, mem));
                break;
            case std::memory_order_acquire:
                append(arm64_assembler::swpa(out, source, mem));
                break;
            case std::memory_order_release:
                append(arm64_assembler::swpl(out, source, mem));
                break;
            case std::memory_order_acq_rel:
                append(arm64_assembler::swpal(out, source, mem));
                break;
            default:
                throw backend_exception("Cannot atomically swap for memory order {}", util::to_underlying(mem_order));
            }
            return;
        default:
            throw backend_exception("Cannot atomically swap type {}", source.type());
        }
    }

    void atomic_cmpxchg(const register_operand& current, const register_operand& acc,
                const register_operand& src, const memory_operand &mem,
                std::memory_order mem_order = default_memory_order)
    {
        if (!asm_.supports_lse()) {
            auto [load_mem_order, store_mem_order] = determine_memory_order(mem_order);
            
            atomic_block(current, mem, [this, &current, &acc]() {
                compare(current, acc);
                append(arm64_assembler::csel(acc, current, acc, cond_operand::ne()))
                    .add_comment("conditionally move current memory scalar into accumulator");
            }, load_mem_order, store_mem_order);
            return;
        }

        insert_comment("Atomic CMPXCHG using CAS (enabled on systems with LSE support");
        switch (mem_order) {
        case std::memory_order_relaxed:
            append(arm64_assembler::cas(acc, src, mem));
            break;
        case std::memory_order_acquire:
            append(arm64_assembler::casa(acc, src, mem));
            break;
        case std::memory_order_release:
            append(arm64_assembler::casl(acc, src, mem));
            break;
        case std::memory_order_acq_rel:
            append(arm64_assembler::casal(acc, src, mem));
            break;
        default:
            throw backend_exception("Cannot perform CAS for memory order of type {}", util::to_underlying(mem_order));
        }

        append(arm64_assembler::cmp(acc, 0));
        append(arm64_assembler::mov(current, acc));

        throw backend_exception("Cannot generate non-exclusive atomic accesses");
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

    void inverse(const register_sequence &out, const register_sequence &source) {
        if (out.type().width() == 1) {
            append(arm64_assembler::eor(out, source, source));
            return;
        }

        for (auto out_it = out.begin(), src_it = source.begin(); out_it != out.end(); ++out_it, ++src_it)
            append(arm64_assembler::mvn(*out_it, *src_it));
    }

    void negate(const register_sequence &out, const register_sequence &source) {
        if (out.type().width() == 1) {
            append(arm64_assembler::eor(out, source, source));
            return;
        }

        append(arm64_assembler::neg(out, source));
    }

    static std::pair<std::memory_order, std::memory_order> 
    determine_memory_order(const std::memory_order& mem_order) {
        std::memory_order load_mem_order;
        std::memory_order store_mem_order;
        switch (mem_order) {
        case std::memory_order_relaxed:
            load_mem_order = std::memory_order_relaxed;
            store_mem_order = std::memory_order_relaxed;
            break;
        case std::memory_order_acquire:
            load_mem_order = std::memory_order_acquire;
            store_mem_order = std::memory_order_relaxed;
            break;
        case std::memory_order_release:
            load_mem_order = std::memory_order_relaxed;
            store_mem_order = std::memory_order_release;
            break;
        case std::memory_order_acq_rel:
            load_mem_order = std::memory_order_acquire;
            store_mem_order = std::memory_order_release;
            break;
        default:
            throw backend_exception("Cannot support memory order {} for atomic operation",
                                    util::to_underlying(mem_order));
        }

        return {load_mem_order, store_mem_order};
    }
};

} // namespace arancini::output::dynamic::arm64
