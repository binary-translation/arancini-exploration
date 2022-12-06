// Copyright (c) 2017, the Newspeak project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#ifndef VM_ASSEMBLER_RISCV_H_
#define VM_ASSEMBLER_RISCV_H_

#include <atomic>

#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/riscv64/machineCode/assert.h>
#include <arancini/output/dynamic/riscv64/machineCode/constants_riscv.h>

namespace arancini::output::dynamic::riscv64 {

class Address {
 public:
  Address(Register base, intptr_t offset) : base_(base), offset_(offset) {}
  explicit Address(Register base) : base_(base), offset_(0) {}

  Register base() const { return base_; }
  intptr_t offset() const { return offset_; }

 private:
  const Register base_;
  const intptr_t offset_;
};

class Label {
 public:
  Label() : position_(0) {}

  ~Label() {
    // Should not be destroyed with unresolved branches pending.
    ASSERT(!IsLinked());
  }

  // Returns the position for bound and linked labels. Cannot be used
  // for unused labels.
  intptr_t Position() const {
    ASSERT(!IsUnused());
    return IsBound() ? -position_ - kWordSize : position_ - kWordSize;
  }

  bool IsBound() const { return position_ < 0; }
  bool IsUnused() const { return position_ == 0; }
  bool IsLinked() const { return position_ > 0; }

 private:
  intptr_t position_;

  void Reinitialize() { position_ = 0; }

  void BindTo(intptr_t position) {
    ASSERT(!IsBound());
    position_ = -position - kWordSize;
    ASSERT(IsBound());
  }

  void LinkTo(intptr_t position) {
    ASSERT(!IsBound());
    position_ = position + kWordSize;
    ASSERT(IsLinked());
  }

  friend class Assembler;
  DISALLOW_COPY_AND_ASSIGN(Label);
};

// All functions produce exactly one instruction.
class Assembler {
 public:
  static const bool kNearJump = true;
  static const bool kFarJump = false;

  explicit Assembler(ExtensionSet extensions = RV_G);
  ~Assembler();

  bool Supports(Extension extension) const {
    return extensions_.Includes(extension);
  }
  bool Supports(ExtensionSet extensions) const {
    return extensions_.IncludesAll(extensions);
  }

  void Bind(Label* label);

  // ==== RV32I ====
  void lui(Register rd, intptr_t imm);
  void auipc(Register rd, intptr_t imm);

  void jal(Register rd, Label* label, bool near = kFarJump);
  void jal(Label* label, bool near = kFarJump) { jal(RA, label, near); }
  void j(Label* label, bool near = kFarJump) { jal(ZERO, label, near); }

  void jalr(Register rd, Register rs1, intptr_t offset = 0);
  void jalr(Register rs1, intptr_t offset = 0) { jalr(RA, rs1, offset); }
  void jr(Register rs1, intptr_t offset = 0) { jalr(ZERO, rs1, offset); }
  void ret() { jalr(ZERO, RA, 0); }

  void beq(Register rs1, Register rs2, Label* label, bool near = kFarJump);
  void bne(Register rs1, Register rs2, Label* label, bool near = kFarJump);
  void blt(Register rs1, Register rs2, Label* label);
  void bge(Register rs1, Register rs2, Label* label);
  void bgt(Register rs1, Register rs2, Label* label) { blt(rs2, rs1, label); }
  void ble(Register rs1, Register rs2, Label* label) { bge(rs2, rs1, label); }
  void bltu(Register rs1, Register rs2, Label* label);
  void bgeu(Register rs1, Register rs2, Label* label);
  void bgtu(Register rs1, Register rs2, Label* label) { bltu(rs2, rs1, label); }
  void bleu(Register rs1, Register rs2, Label* label) { bgeu(rs2, rs1, label); }

  void lb(Register rd, Address addr);
  void lh(Register rd, Address addr);
  void lw(Register rd, Address addr);
  void lbu(Register rd, Address addr);
  void lhu(Register rd, Address addr);

  void sb(Register rs2, Address addr);
  void sh(Register rs2, Address addr);
  void sw(Register rs2, Address addr);

  void addi(Register rd, Register rs1, intptr_t imm);
  void subi(Register rd, Register rs1, intptr_t imm) { addi(rd, rs1, -imm); }
  void slti(Register rd, Register rs1, intptr_t imm);
  void sltiu(Register rd, Register rs1, intptr_t imm);
  void xori(Register rd, Register rs1, intptr_t imm);
  void ori(Register rd, Register rs1, intptr_t imm);
  void andi(Register rd, Register rs1, intptr_t imm);
  void slli(Register rd, Register rs1, intptr_t shamt);
  void srli(Register rd, Register rs1, intptr_t shamt);
  void srai(Register rd, Register rs1, intptr_t shamt);

  void add(Register rd, Register rs1, Register rs2);
  void sub(Register rd, Register rs1, Register rs2);
  void sll(Register rd, Register rs1, Register rs2);
  void slt(Register rd, Register rs1, Register rs2);
  void sltu(Register rd, Register rs1, Register rs2);
  void xor_(Register rd, Register rs1, Register rs2);
  void srl(Register rd, Register rs1, Register rs2);
  void sra(Register rd, Register rs1, Register rs2);
  void or_(Register rd, Register rs1, Register rs2);
  void and_(Register rd, Register rs1, Register rs2);

  void fence(HartEffects predecessor, HartEffects successor);
  void fence() { fence(kAll, kAll); }
  void fencei();
  void ecall();
  void ebreak();

  void csrrw(Register rd, uint32_t csr, Register rs1);
  void csrrs(Register rd, uint32_t csr, Register rs1);
  void csrrc(Register rd, uint32_t csr, Register rs1);
  void csrr(Register rd, uint32_t csr) { csrrs(rd, csr, ZERO); }
  void csrw(uint32_t csr, Register rs) { csrrw(ZERO, csr, rs); }
  void csrs(uint32_t csr, Register rs) { csrrs(ZERO, csr, rs); }
  void csrc(uint32_t csr, Register rs) { csrrc(ZERO, csr, rs); }
  void csrrwi(Register rd, uint32_t csr, uint32_t imm);
  void csrrsi(Register rd, uint32_t csr, uint32_t imm);
  void csrrci(Register rd, uint32_t csr, uint32_t imm);
  void csrwi(uint32_t csr, uint32_t imm) { csrrwi(ZERO, csr, imm); }
  void csrsi(uint32_t csr, uint32_t imm) { csrrsi(ZERO, csr, imm); }
  void csrci(uint32_t csr, uint32_t imm) { csrrci(ZERO, csr, imm); }

  void trap();  // Permanently reserved illegal instruction.

  void nop() { addi(ZERO, ZERO, 0); }
  void li(Register rd, intptr_t imm) { addi(rd, ZERO, imm); }
  void mv(Register rd, Register rs) { addi(rd, rs, 0); }
  void not_(Register rd, Register rs) { xori(rd, rs, -1); }
  void neg(Register rd, Register rs) { sub(rd, ZERO, rs); }

  void snez(Register rd, Register rs) { sltu(rd, ZERO, rs); }
  void seqz(Register rd, Register rs) { sltiu(rd, rs, 1); }
  void sltz(Register rd, Register rs) { slt(rd, rs, ZERO); }
  void sgtz(Register rd, Register rs) { slt(rd, ZERO, rs); }

  void beqz(Register rs, Label* label, bool near = kFarJump) {
    beq(rs, ZERO, label, near);
  }
  void bnez(Register rs, Label* label, bool near = kFarJump) {
    bne(rs, ZERO, label, near);
  }
  void blez(Register rs, Label* label) { bge(ZERO, rs, label); }
  void bgez(Register rs, Label* label) { bge(rs, ZERO, label); }
  void bltz(Register rs, Label* label) { blt(rs, ZERO, label); }
  void bgtz(Register rs, Label* label) { blt(ZERO, rs, label); }

  // ==== RV64I ====
#if XLEN >= 64
  void lwu(Register rd, Address addr);
  void ld(Register rd, Address addr);

  void sd(Register rs2, Address addr);

  void addiw(Register rd, Register rs1, intptr_t imm);
  void subiw(Register rd, Register rs1, intptr_t imm) { addi(rd, rs1, -imm); }
  void slliw(Register rd, Register rs1, intptr_t shamt);
  void srliw(Register rd, Register rs1, intptr_t shamt);
  void sraiw(Register rd, Register rs1, intptr_t shamt);

  void addw(Register rd, Register rs1, Register rs2);
  void subw(Register rd, Register rs1, Register rs2);
  void sllw(Register rd, Register rs1, Register rs2);
  void srlw(Register rd, Register rs1, Register rs2);
  void sraw(Register rd, Register rs1, Register rs2);

  void negw(Register rd, Register rs) { subw(rd, ZERO, rs); }
  void sextw(Register rd, Register rs) { addiw(rd, rs, 0); }
#endif  // XLEN >= 64

#if XLEN == 32
  void lx(Register rd, Address addr) { lw(rd, addr); }
  void sx(Register rs2, Address addr) { sw(rs2, addr); }
#elif XLEN == 64
  void lx(Register rd, Address addr) { ld(rd, addr); }
  void sx(Register rs2, Address addr) { sd(rs2, addr); }
#elif XLEN == 128
  void lx(Register rd, Address addr) { lq(rd, addr); }
  void sx(Register rs2, Address addr) { sq(rs2, addr); }
#endif

  // ==== RV32M ====
  void mul(Register rd, Register rs1, Register rs2);
  void mulh(Register rd, Register rs1, Register rs2);
  void mulhsu(Register rd, Register rs1, Register rs2);
  void mulhu(Register rd, Register rs1, Register rs2);
  void div(Register rd, Register rs1, Register rs2);
  void divu(Register rd, Register rs1, Register rs2);
  void rem(Register rd, Register rs1, Register rs2);
  void remu(Register rd, Register rs1, Register rs2);

  // ==== RV64M ====
#if XLEN >= 64
  void mulw(Register rd, Register rs1, Register rs2);
  void divw(Register rd, Register rs1, Register rs2);
  void divuw(Register rd, Register rs1, Register rs2);
  void remw(Register rd, Register rs1, Register rs2);
  void remuw(Register rd, Register rs1, Register rs2);
#endif  // XLEN >= 64

  // ==== RV32A ====
  void lrw(Register rd,
           Address addr,
           std::memory_order order = std::memory_order_relaxed);
  void scw(Register rd,
           Register rs2,
           Address addr,
           std::memory_order order = std::memory_order_relaxed);
  void amoswapw(Register rd,
                Register rs2,
                Address addr,
                std::memory_order order = std::memory_order_relaxed);
  void amoaddw(Register rd,
               Register rs2,
               Address addr,
               std::memory_order order = std::memory_order_relaxed);
  void amoxorw(Register rd,
               Register rs2,
               Address addr,
               std::memory_order order = std::memory_order_relaxed);
  void amoandw(Register rd,
               Register rs2,
               Address addr,
               std::memory_order order = std::memory_order_relaxed);
  void amoorw(Register rd,
              Register rs2,
              Address addr,
              std::memory_order order = std::memory_order_relaxed);
  void amominw(Register rd,
               Register rs2,
               Address addr,
               std::memory_order order = std::memory_order_relaxed);
  void amomaxw(Register rd,
               Register rs2,
               Address addr,
               std::memory_order order = std::memory_order_relaxed);
  void amominuw(Register rd,
                Register rs2,
                Address addr,
                std::memory_order order = std::memory_order_relaxed);
  void amomaxuw(Register rd,
                Register rs2,
                Address addr,
                std::memory_order order = std::memory_order_relaxed);

  // ==== RV64A ====
#if XLEN >= 64
  void lrd(Register rd,
           Address addr,
           std::memory_order order = std::memory_order_relaxed);
  void scd(Register rd,
           Register rs2,
           Address addr,
           std::memory_order order = std::memory_order_relaxed);
  void amoswapd(Register rd,
                Register rs2,
                Address addr,
                std::memory_order order = std::memory_order_relaxed);
  void amoaddd(Register rd,
               Register rs2,
               Address addr,
               std::memory_order order = std::memory_order_relaxed);
  void amoxord(Register rd,
               Register rs2,
               Address addr,
               std::memory_order order = std::memory_order_relaxed);
  void amoandd(Register rd,
               Register rs2,
               Address addr,
               std::memory_order order = std::memory_order_relaxed);
  void amoord(Register rd,
              Register rs2,
              Address addr,
              std::memory_order order = std::memory_order_relaxed);
  void amomind(Register rd,
               Register rs2,
               Address addr,
               std::memory_order order = std::memory_order_relaxed);
  void amomaxd(Register rd,
               Register rs2,
               Address addr,
               std::memory_order order = std::memory_order_relaxed);
  void amominud(Register rd,
                Register rs2,
                Address addr,
                std::memory_order order = std::memory_order_relaxed);
  void amomaxud(Register rd,
                Register rs2,
                Address addr,
                std::memory_order order = std::memory_order_relaxed);
#endif  // XLEN >= 64

#if XLEN == 32
  void lr(Register rd,
          Address addr,
          std::memory_order order = std::memory_order_relaxed) {
    lrw(rd, addr, order);
  }
  void sr(Register rd,
          Register rs2,
          Address addr,
          std::memory_order order = std::memory_order_relaxed) {
    scw(rd, rs2, addr, order);
  }
#elif XLEN == 64
  void lr(Register rd,
          Address addr,
          std::memory_order order = std::memory_order_relaxed) {
    lrd(rd, addr, order);
  }
  void sr(Register rd,
          Register rs2,
          Address addr,
          std::memory_order order = std::memory_order_relaxed) {
    scd(rd, rs2, addr, order);
  }
#elif XLEN == 128
  void lr(Register rd,
          Address addr,
          std::memory_order order = std::memory_order_relaxed) {
    lrq(rd, addr, order);
  }
  void sr(Register rd,
          Register rs2,
          Address addr,
          std::memory_order order = std::memory_order_relaxed) {
    scq(rd, rs2, addr, order);
  }
#endif

  // ==== RV32F ====
  void flw(FRegister rd, Address addr);
  void fsw(FRegister rs2, Address addr);
  // rd := (rs1 * rs2) + rs3
  void fmadds(FRegister rd,
              FRegister rs1,
              FRegister rs2,
              FRegister rs3,
              RoundingMode rounding = RNE);
  // rd := (rs1 * rs2) - rs3
  void fmsubs(FRegister rd,
              FRegister rs1,
              FRegister rs2,
              FRegister rs3,
              RoundingMode rounding = RNE);
  // rd := -(rs1 * rs2) + rs3
  void fnmsubs(FRegister rd,
               FRegister rs1,
               FRegister rs2,
               FRegister rs3,
               RoundingMode rounding = RNE);
  // rd := -(rs1 * rs2) - rs3
  void fnmadds(FRegister rd,
               FRegister rs1,
               FRegister rs2,
               FRegister rs3,
               RoundingMode rounding = RNE);
  void fadds(FRegister rd,
             FRegister rs1,
             FRegister rs2,
             RoundingMode rounding = RNE);
  void fsubs(FRegister rd,
             FRegister rs1,
             FRegister rs2,
             RoundingMode rounding = RNE);
  void fmuls(FRegister rd,
             FRegister rs1,
             FRegister rs2,
             RoundingMode rounding = RNE);
  void fdivs(FRegister rd,
             FRegister rs1,
             FRegister rs2,
             RoundingMode rounding = RNE);
  void fsqrts(FRegister rd, FRegister rs1, RoundingMode rounding = RNE);
  void fsgnjs(FRegister rd, FRegister rs1, FRegister rs2);
  void fsgnjns(FRegister rd, FRegister rs1, FRegister rs2);
  void fsgnjxs(FRegister rd, FRegister rs1, FRegister rs2);
  void fmins(FRegister rd, FRegister rs1, FRegister rs2);
  void fmaxs(FRegister rd, FRegister rs1, FRegister rs2);
  void feqs(Register rd, FRegister rs1, FRegister rs2);
  void flts(Register rd, FRegister rs1, FRegister rs2);
  void fles(Register rd, FRegister rs1, FRegister rs2);
  void fclasss(Register rd, FRegister rs1);
  // int32_t <- float
  void fcvtws(Register rd, FRegister rs1, RoundingMode rounding = RNE);
  // uint32_t <- float
  void fcvtwus(Register rd, FRegister rs1, RoundingMode rounding = RNE);
  // float <- int32_t
  void fcvtsw(FRegister rd, Register rs1, RoundingMode rounding = RNE);
  // float <- uint32_t
  void fcvtswu(FRegister rd, Register rs1, RoundingMode rounding = RNE);

  void fmvs(FRegister rd, FRegister rs) { fsgnjs(rd, rs, rs); }
  void fabss(FRegister rd, FRegister rs) { fsgnjxs(rd, rs, rs); }
  void fnegs(FRegister rd, FRegister rs) { fsgnjns(rd, rs, rs); }

  // xlen <--bit_cast-- float
  void fmvxw(Register rd, FRegister rs1);
  // float <--bit_cast-- xlen
  void fmvwx(FRegister rd, Register rs1);

  // ==== RV64F ====
#if XLEN >= 64
  // int64_t <- double
  void fcvtls(Register rd, FRegister rs1, RoundingMode rounding = RNE);
  // uint64_t <- double
  void fcvtlus(Register rd, FRegister rs1, RoundingMode rounding = RNE);
  // double <- int64_t
  void fcvtsl(FRegister rd, Register rs1, RoundingMode rounding = RNE);
  // double <- uint64_t
  void fcvtslu(FRegister rd, Register rs1, RoundingMode rounding = RNE);
#endif  // XLEN >= 64

  // ==== RV32D ====
  void fld(FRegister rd, Address addr);
  void fsd(FRegister rs2, Address addr);
  // rd := (rs1 * rs2) + rs3
  void fmaddd(FRegister rd,
              FRegister rs1,
              FRegister rs2,
              FRegister rs3,
              RoundingMode rounding = RNE);
  // rd := (rs1 * rs2) - rs3
  void fmsubd(FRegister rd,
              FRegister rs1,
              FRegister rs2,
              FRegister rs3,
              RoundingMode rounding = RNE);
  // rd := -(rs1 * rs2) - rs3
  void fnmsubd(FRegister rd,
               FRegister rs1,
               FRegister rs2,
               FRegister rs3,
               RoundingMode rounding = RNE);
  // rd := -(rs1 * rs2) + rs3
  void fnmaddd(FRegister rd,
               FRegister rs1,
               FRegister rs2,
               FRegister rs3,
               RoundingMode rounding = RNE);
  void faddd(FRegister rd,
             FRegister rs1,
             FRegister rs2,
             RoundingMode rounding = RNE);
  void fsubd(FRegister rd,
             FRegister rs1,
             FRegister rs2,
             RoundingMode rounding = RNE);
  void fmuld(FRegister rd,
             FRegister rs1,
             FRegister rs2,
             RoundingMode rounding = RNE);
  void fdivd(FRegister rd,
             FRegister rs1,
             FRegister rs2,
             RoundingMode rounding = RNE);
  void fsqrtd(FRegister rd, FRegister rs1, RoundingMode rounding = RNE);
  void fsgnjd(FRegister rd, FRegister rs1, FRegister rs2);
  void fsgnjnd(FRegister rd, FRegister rs1, FRegister rs2);
  void fsgnjxd(FRegister rd, FRegister rs1, FRegister rs2);
  void fmind(FRegister rd, FRegister rs1, FRegister rs2);
  void fmaxd(FRegister rd, FRegister rs1, FRegister rs2);
  void fcvtsd(FRegister rd, FRegister rs1, RoundingMode rounding = RNE);
  void fcvtds(FRegister rd, FRegister rs1, RoundingMode rounding = RNE);
  void feqd(Register rd, FRegister rs1, FRegister rs2);
  void fltd(Register rd, FRegister rs1, FRegister rs2);
  void fled(Register rd, FRegister rs1, FRegister rs2);
  void fclassd(Register rd, FRegister rs1);
  // int32_t <- double
  void fcvtwd(Register rd, FRegister rs1, RoundingMode rounding = RNE);
  // uint32_t <- double
  void fcvtwud(Register rd, FRegister rs1, RoundingMode rounding = RNE);
  // double <- int32_t
  void fcvtdw(FRegister rd, Register rs1, RoundingMode rounding = RNE);
  // double <- uint32_t
  void fcvtdwu(FRegister rd, Register rs1, RoundingMode rounding = RNE);

  void fmvd(FRegister rd, FRegister rs) { fsgnjd(rd, rs, rs); }
  void fabsd(FRegister rd, FRegister rs) { fsgnjxd(rd, rs, rs); }
  void fnegd(FRegister rd, FRegister rs) { fsgnjnd(rd, rs, rs); }

  // ==== RV64D ====
#if XLEN >= 64
  // int64_t <- double
  void fcvtld(Register rd, FRegister rs1, RoundingMode rounding = RNE);
  // uint64_t <- double
  void fcvtlud(Register rd, FRegister rs1, RoundingMode rounding = RNE);
  // xlen <--bit_cast-- double
  void fmvxd(Register rd, FRegister rs1);
  // double <- int64_t
  void fcvtdl(FRegister rd, Register rs1, RoundingMode rounding = RNE);
  // double <- uint64_t
  void fcvtdlu(FRegister rd, Register rs1, RoundingMode rounding = RNE);
  // double <--bit_cast-- xlen
  void fmvdx(FRegister rd, Register rs1);
#endif  // XLEN >= 64

  // ==== Zba: Address generation ====
  void adduw(Register rd, Register rs1, Register rs2);
  void sh1add(Register rd, Register rs1, Register rs2);
  void sh1adduw(Register rd, Register rs1, Register rs2);
  void sh2add(Register rd, Register rs1, Register rs2);
  void sh2adduw(Register rd, Register rs1, Register rs2);
  void sh3add(Register rd, Register rs1, Register rs2);
  void sh3adduw(Register rd, Register rs1, Register rs2);
  void slliuw(Register rd, Register rs1, intx_t imm);

  // ==== Zbb: Basic bit-manipulation ====
  void andn(Register rd, Register rs1, Register rs2);
  void orn(Register rd, Register rs1, Register rs2);
  void xnor(Register rd, Register rs1, Register rs2);
  void clz(Register rd, Register rs);
  void clzw(Register rd, Register rs);
  void ctz(Register rd, Register rs);
  void ctzw(Register rd, Register rs);
  void cpop(Register rd, Register rs);
  void cpopw(Register rd, Register rs);
  void max(Register rd, Register rs1, Register rs2);
  void maxu(Register rd, Register rs1, Register rs2);
  void min(Register rd, Register rs1, Register rs2);
  void minu(Register rd, Register rs1, Register rs2);
  void sextb(Register rd, Register rs);
  void sexth(Register rd, Register rs);
  void zexth(Register rd, Register rs);
  void rol(Register rd, Register rs1, Register rs2);
  void rolw(Register rd, Register rs1, Register rs2);
  void ror(Register rd, Register rs1, Register rs2);
  void rori(Register rd, Register rs1, intx_t imm);
  void roriw(Register rd, Register rs1, intx_t imm);
  void rorw(Register rd, Register rs1, Register rs2);
  void orcb(Register rd, Register rs);
  void rev8(Register rd, Register rs);

  // ==== Zbc: Carry-less multiplication ====
  void clmul(Register rd, Register rs1, Register rs2);
  void clmulh(Register rd, Register rs1, Register rs2);
  void clmulr(Register rd, Register rs1, Register rs2);

  // ==== Zbs: Single-bit instructions ====
  void bclr(Register rd, Register rs1, Register rs2);
  void bclri(Register rd, Register rs1, intx_t shamt);
  void bext(Register rd, Register rs1, Register rs2);
  void bexti(Register rd, Register rs1, intx_t shamt);
  void binv(Register rd, Register rs1, Register rs2);
  void binvi(Register rd, Register rs1, intx_t shamt);
  void bset(Register rd, Register rs1, Register rs2);
  void bseti(Register rd, Register rs1, intx_t shamt);

 private:
  // ==== RV32/64C ====
  void c_lwsp(Register rd, Address addr);
#if XLEN == 32
  void c_flwsp(FRegister rd, Address addr);
#else
  void c_ldsp(Register rd, Address addr);
#endif
  void c_fldsp(FRegister rd, Address addr);

  void c_swsp(Register rs2, Address addr);
#if XLEN == 32
  void c_fswsp(FRegister rs2, Address addr);
#else
  void c_sdsp(Register rs2, Address addr);
#endif
  void c_fsdsp(FRegister rs2, Address addr);

  void c_lw(Register rd, Address addr);
  void c_ld(Register rd, Address addr);
  void c_flw(FRegister rd, Address addr);
  void c_fld(FRegister rd, Address addr);

  void c_sw(Register rs2, Address addr);
  void c_sd(Register rs2, Address addr);
  void c_fsw(FRegister rs2, Address addr);
  void c_fsd(FRegister rs2, Address addr);

  void c_j(Label* label);
#if XLEN == 32
  void c_jal(Label* label);
#endif
  void c_jr(Register rs1);
  void c_jalr(Register rs1);

  void c_beqz(Register rs1p, Label* label);
  void c_bnez(Register rs1p, Label* label);

  void c_li(Register rd, intptr_t imm);
  void c_lui(Register rd, uintptr_t imm);

  void c_addi(Register rd, Register rs1, intptr_t imm);
#if XLEN >= 64
  void c_addiw(Register rd, Register rs1, intptr_t imm);
#endif
  void c_addi16sp(Register rd, Register rs1, intptr_t imm);
  void c_addi4spn(Register rdp, Register rs1, intptr_t imm);

  void c_slli(Register rd, Register rs1, intptr_t imm);
  void c_srli(Register rd, Register rs1, intptr_t imm);
  void c_srai(Register rd, Register rs1, intptr_t imm);
  void c_andi(Register rd, Register rs1, intptr_t imm);

  void c_mv(Register rd, Register rs2);

  void c_add(Register rd, Register rs1, Register rs2);
  void c_and(Register rd, Register rs1, Register rs2);
  void c_or(Register rd, Register rs1, Register rs2);
  void c_xor(Register rd, Register rs1, Register rs2);
  void c_sub(Register rd, Register rs1, Register rs2);
#if XLEN >= 64
  void c_addw(Register rd, Register rs1, Register rs2);
  void c_subw(Register rd, Register rs1, Register rs2);
#endif

  void c_nop();
  void c_ebreak();

  uint32_t EncodeBranchOrJumpOffset(int32_t offset, uint32_t encoded);
  int32_t DecodeBranchOrJumpOffset(uint32_t encoded);
  uint32_t EncodeCBranchOrJumpOffset(int32_t offset, uint32_t encoded);
  int32_t DecodeCBranchOrJumpOffset(uint32_t encoded);

  intptr_t Position() { return writer.size(); }
  void EmitBranch(Register rs1, Register rs2, Label* label, Funct3 func);
  void EmitJump(Register rd, Label* label, Opcode op);
  void EmitCBranch(Register rs1p, Label* label, COpcode op);
  void EmitCJump(Label* label, COpcode op);

  void EmitRType(Funct5 funct5,
                 std::memory_order order,
                 Register rs2,
                 Register rs1,
                 Funct3 funct3,
                 Register rd,
                 Opcode opcode);
  void EmitRType(Funct7 funct7,
                 Register rs2,
                 Register rs1,
                 Funct3 funct3,
                 Register rd,
                 Opcode opcode);
  void EmitRType(Funct7 funct7,
                 FRegister rs2,
                 FRegister rs1,
                 Funct3 funct3,
                 FRegister rd,
                 Opcode opcode);
  void EmitRType(Funct7 funct7,
                 FRegister rs2,
                 FRegister rs1,
                 RoundingMode round,
                 FRegister rd,
                 Opcode opcode);
  void EmitRType(Funct7 funct7,
                 FRegister rs2,
                 Register rs1,
                 RoundingMode round,
                 FRegister rd,
                 Opcode opcode);
  void EmitRType(Funct7 funct7,
                 FRegister rs2,
                 Register rs1,
                 Funct3 funct3,
                 FRegister rd,
                 Opcode opcode);
  void EmitRType(Funct7 funct7,
                 FRegister rs2,
                 FRegister rs1,
                 Funct3 funct3,
                 Register rd,
                 Opcode opcode);
  void EmitRType(Funct7 funct7,
                 FRegister rs2,
                 FRegister rs1,
                 RoundingMode round,
                 Register rd,
                 Opcode opcode);
  void EmitRType(Funct7 funct7,
                 intptr_t shamt,
                 Register rs1,
                 Funct3 funct3,
                 Register rd,
                 Opcode opcode);

  void EmitR4Type(FRegister rs3,
                  Funct2 funct2,
                  FRegister rs2,
                  FRegister rs1,
                  RoundingMode round,
                  FRegister rd,
                  Opcode opcode);

  void EmitIType(intptr_t imm,
                 Register rs1,
                 Funct3 funct3,
                 Register rd,
                 Opcode opcode);
  void EmitIType(intptr_t imm,
                 Register rs1,
                 Funct3 funct3,
                 FRegister rd,
                 Opcode opcode);

  void EmitSType(intptr_t imm,
                 Register rs2,
                 Register rs1,
                 Funct3 funct3,
                 Opcode opcode);
  void EmitSType(intptr_t imm,
                 FRegister rs2,
                 Register rs1,
                 Funct3 funct3,
                 Opcode opcode);

  void EmitBType(intptr_t imm,
                 Register rs2,
                 Register rs1,
                 Funct3 funct3,
                 Opcode opcode);

  void EmitUType(intptr_t imm, Register rd, Opcode opcode);

  void EmitJType(intptr_t imm, Register rd, Opcode opcode);

  uint32_t Read32(intptr_t position) {
    return *reinterpret_cast<uint32_t*>(reinterpret_cast<uword>(writer.ptr()) +
                                        position);
  }
  void Write32(intptr_t position, uint32_t instruction) {
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uword>(writer.ptr()) + position) =
        instruction;
  }
  void Emit32(uint32_t instruction) { writer.emit32(instruction); }
  uint16_t Read16(intptr_t position) {
    return *reinterpret_cast<uint16_t*>(reinterpret_cast<uword>(writer.ptr()) +
                                        position);
  }
  void Write16(intptr_t position, uint16_t instruction) {
    *reinterpret_cast<uint16_t*>(reinterpret_cast<uword>(writer.ptr()) + position) =
        instruction;
  }
  void Emit16(uint16_t instruction) { writer.emit16(instruction); }

  const ExtensionSet extensions_;
  arancini::output::dynamic::machine_code_writer writer;
};

}  // namespace arancini::output::dynamic::riscv64

#endif  // VM_ASSEMBLER_RISCV_H_
