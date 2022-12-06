// Copyright (c) 2017, the Newspeak project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#ifndef VM_CONSTANTS_RISCV_H_
#define VM_CONSTANTS_RISCV_H_

//TODO
#define XLEN 64

#include <atomic>
#include <sstream>

#include <arancini/output/dynamic/riscv64/machineCode/assert.h>
#include <arancini/output/dynamic/riscv64/machineCode/globals.h>
#include <arancini/output/dynamic/riscv64/machineCode/utils.h>

namespace arancini::output::dynamic::riscv64 {

// The RISC-V Instruction Set Manual, Volume I: User-Level ISA, Document Version
// 2.2, Editors Andrew Waterman and Krste Asanovic, RISC-V Foundation, May 2017

#if __SIZEOF_POINTER__ == 4
#define HOST_XLEN 32
#elif __SIZEOF_POINTER__ == 8
#define HOST_XLEN 64
#else
#error What size?
#endif

constexpr uint32_t kMaxUInt32 = 0xFFFFFFFF;
constexpr uint64_t kMaxUInt64 = 0xFFFFFFFFFFFFFFFF;

#if XLEN == 32
typedef uint32_t uintx_t;
typedef int32_t intx_t;
constexpr intx_t kMaxIntX = kMaxInt32;
constexpr uintx_t kMaxUIntX = kMaxUInt32;
constexpr intx_t kMinIntX = kMinInt32;
#elif XLEN == 64
typedef uint64_t uintx_t;
typedef int64_t intx_t;
constexpr intx_t kMaxIntX = kMaxInt64;
constexpr uintx_t kMaxUIntX = kMaxUInt64;
constexpr intx_t kMinIntX = kMinInt64;
#else
#error Invalid XLEN
#endif

inline int32_t SignExtend(int N, int32_t value) {
  return (value << (32 - N)) >> (32 - N);
}

inline intx_t sign_extend(int32_t x) {
  return static_cast<intx_t>(x);
}
inline intx_t sign_extend(int64_t x) {
  return static_cast<intx_t>(x);
}
inline intx_t sign_extend(uint32_t x) {
  return static_cast<intx_t>(static_cast<int32_t>(x));
}
inline intx_t sign_extend(uint64_t x) {
  return static_cast<intx_t>(static_cast<int64_t>(x));
}

#define DEFINE_TYPED_ENUM_SET(name, storage_t)                                 \
  class name##Set;                                                             \
  class name {                                                                 \
   public:                                                                     \
    constexpr explicit name(storage_t encoding) : encoding_(encoding) {}       \
    constexpr storage_t encoding() const { return encoding_; }                 \
    constexpr bool operator==(const name& other) const {                       \
      return encoding_ == other.encoding_;                                     \
    }                                                                          \
    constexpr bool operator!=(const name& other) const {                       \
      return encoding_ != other.encoding_;                                     \
    }                                                                          \
    inline constexpr name##Set operator|(const name& other) const;             \
    inline constexpr name##Set operator|(const name##Set& other) const;        \
                                                                               \
   private:                                                                    \
    const storage_t encoding_;                                                 \
  };                                                                           \
  inline std::ostream& operator<<(std::ostream& stream, const name& element) { \
    return stream << #name << "(" << element.encoding() << ")";                \
  }                                                                            \
  class name##Set {                                                            \
   public:                                                                     \
    constexpr /* implicit */ name##Set(name element)                           \
        : encoding_(1u << element.encoding()) {}                               \
    constexpr explicit name##Set(storage_t encoding) : encoding_(encoding) {}  \
    constexpr static name##Set Empty() { return name##Set(0); }                \
    constexpr bool Includes(const name r) const {                              \
      return (encoding_ & (1 << r.encoding())) != 0;                           \
    }                                                                          \
    constexpr bool IncludesAll(const name##Set other) const {                  \
      return (encoding_ & other.encoding_) == other.encoding_;                 \
    }                                                                          \
    constexpr bool IsEmpty() const { return encoding_ == 0; }                  \
    constexpr bool operator==(const name##Set& other) const {                  \
      return encoding_ == other.encoding_;                                     \
    }                                                                          \
    constexpr bool operator!=(const name##Set& other) const {                  \
      return encoding_ != other.encoding_;                                     \
    }                                                                          \
    constexpr name##Set operator|(const name& other) const {                   \
      return name##Set(encoding_ | (1 << other.encoding()));                   \
    }                                                                          \
    constexpr name##Set operator|(const name##Set& other) const {              \
      return name##Set(encoding_ | other.encoding_);                           \
    }                                                                          \
    constexpr name##Set operator&(const name##Set& other) const {              \
      return name##Set(encoding_ & other.encoding_);                           \
    }                                                                          \
                                                                               \
   private:                                                                    \
    const storage_t encoding_;                                                 \
  };                                                                           \
  constexpr name##Set name::operator|(const name& other) const {               \
    return name##Set((1u << encoding_) | (1u << other.encoding_));             \
  }                                                                            \
  constexpr name##Set name::operator|(const name##Set& other) const {          \
    return other | *this;                                                      \
  }

DEFINE_TYPED_ENUM_SET(Register, uint32_t)
static constexpr Register ZERO(0);
static constexpr Register RA(1);
static constexpr Register SP(2);
static constexpr Register GP(3);
static constexpr Register TP(4);
static constexpr Register T0(5);  // aka RA2
static constexpr Register T1(6);
static constexpr Register T2(7);
static constexpr Register FP(8);  // aka S0
static constexpr Register S1(9);
static constexpr Register A0(10);
static constexpr Register A1(11);
static constexpr Register A2(12);
static constexpr Register A3(13);
static constexpr Register A4(14);
static constexpr Register A5(15);
static constexpr Register A6(16);
static constexpr Register A7(17);
static constexpr Register S2(18);
static constexpr Register S3(19);
static constexpr Register S4(20);
static constexpr Register S5(21);
static constexpr Register S6(22);
static constexpr Register S7(23);
static constexpr Register S8(24);
static constexpr Register S9(25);
static constexpr Register S10(26);
static constexpr Register S11(27);
static constexpr Register T3(28);
static constexpr Register T4(29);
static constexpr Register T5(30);
static constexpr Register T6(31);

static constexpr Register RA2 = T0;
static constexpr Register S0 = FP;

static const intptr_t kNumRegisters = 32;
static const char* const kRegisterNames[kNumRegisters] = {
    "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "fp", "s1", "a0",
    "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
    "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

static constexpr RegisterSet kVolatileRegisters = RA | A0 | A1 | A2 | A3 | A4 |
                                                  A5 | A6 | A7 | T0 | T1 | T2 |
                                                  T3 | T4 | T5 | T6;
static constexpr RegisterSet kPreservedRegisters =
    S1 | S2 | S3 | S4 | S5 | S6 | S7 | S8 | S9 | S10 | S11;
static constexpr RegisterSet kReservedRegisters = ZERO | SP | GP | TP | FP;
static constexpr RegisterSet kAllRegisters = RegisterSet(0xFFFFFFFF);

COMPILE_ASSERT((kVolatileRegisters | kPreservedRegisters |
                kReservedRegisters) == kAllRegisters);
COMPILE_ASSERT((kVolatileRegisters & kPreservedRegisters).IsEmpty());
COMPILE_ASSERT((kPreservedRegisters & kReservedRegisters).IsEmpty());
COMPILE_ASSERT((kReservedRegisters & kPreservedRegisters).IsEmpty());

DEFINE_TYPED_ENUM_SET(FRegister, uint32_t)
static constexpr FRegister FT0(0);
static constexpr FRegister FT1(1);
static constexpr FRegister FT2(2);
static constexpr FRegister FT3(3);
static constexpr FRegister FT4(4);
static constexpr FRegister FT5(5);
static constexpr FRegister FT6(6);
static constexpr FRegister FT7(7);
static constexpr FRegister FS0(8);
static constexpr FRegister FS1(9);
static constexpr FRegister FA0(10);
static constexpr FRegister FA1(11);
static constexpr FRegister FA2(12);
static constexpr FRegister FA3(13);
static constexpr FRegister FA4(14);
static constexpr FRegister FA5(15);
static constexpr FRegister FA6(16);
static constexpr FRegister FA7(17);
static constexpr FRegister FS2(18);
static constexpr FRegister FS3(19);
static constexpr FRegister FS4(20);
static constexpr FRegister FS5(21);
static constexpr FRegister FS6(22);
static constexpr FRegister FS7(23);
static constexpr FRegister FS8(24);
static constexpr FRegister FS9(25);
static constexpr FRegister FS10(26);
static constexpr FRegister FS11(27);
static constexpr FRegister FT8(28);
static constexpr FRegister FT9(29);
static constexpr FRegister FT10(30);
static constexpr FRegister FT11(31);

static const intptr_t kNumFRegisters = 32;
static const char* const kFRegisterNames[kNumFRegisters] = {
    "ft0", "ft1", "ft2",  "ft3",  "ft4", "ft5", "ft6",  "ft7",
    "fs0", "fs1", "fa0",  "fa1",  "fa2", "fa3", "fa4",  "fa5",
    "fa6", "fa7", "fs2",  "fs3",  "fs4", "fs5", "fs6",  "fs7",
    "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11",
};

static constexpr FRegisterSet kVolatileFRegisters =
    FA0 | FA1 | FA2 | FA3 | FA4 | FA5 | FA6 | FA7 | FT0 | FT1 | FT2 | FT3 |
    FT4 | FT5 | FT6 | FT7 | FT8 | FT9 | FT10 | FT11;
static constexpr FRegisterSet kPreservedFRegisters =
    FS0 | FS1 | FS2 | FS3 | FS4 | FS5 | FS6 | FS7 | FS8 | FS9 | FS10 | FS11;
static constexpr FRegisterSet kAllFRegisters = FRegisterSet(0xFFFFFFFF);

COMPILE_ASSERT((kVolatileFRegisters | kPreservedFRegisters) == kAllFRegisters);
COMPILE_ASSERT((kVolatileFRegisters & kPreservedFRegisters).IsEmpty());

enum Opcode {
  LUI = 0b0110111,
  AUIPC = 0b0010111,
  JAL = 0b1101111,
  JALR = 0b1100111,
  BRANCH = 0b1100011,
  LOAD = 0b0000011,
  STORE = 0b0100011,
  OPIMM = 0b0010011,
  OP = 0b0110011,
  MISCMEM = 0b0001111,
  SYSTEM = 0b1110011,
  OP32 = 0b0111011,
  OPIMM32 = 0b0011011,
  AMO = 0b0101111,
  LOADFP = 0b0000111,
  STOREFP = 0b0100111,
  FMADD = 0b1000011,
  FMSUB = 0b1000111,
  FNMSUB = 0b1001011,
  FNMADD = 0b1001111,
  OPFP = 0b1010011,
};

enum Funct12 {
  ECALL = 0,
  EBREAK = 1,
};

enum Funct3 {
  F3_0 = 0,
  F3_1 = 1,

  BEQ = 0b000,
  BNE = 0b001,
  BLT = 0b100,
  BGE = 0b101,
  BLTU = 0b110,
  BGEU = 0b111,

  LB = 0b000,
  LH = 0b001,
  LW = 0b010,
  LBU = 0b100,
  LHU = 0b101,
  LWU = 0b110,
  LD = 0b011,

  SB = 0b000,
  SH = 0b001,
  SW = 0b010,
  SD = 0b011,

  ADDI = 0b000,
  SLLI = 0b001,
  SLTI = 0b010,
  SLTIU = 0b011,
  XORI = 0b100,
  SRI = 0b101,
  ORI = 0b110,
  ANDI = 0b111,

  ADD = 0b000,
  SLL = 0b001,
  SLT = 0b010,
  SLTU = 0b011,
  XOR = 0b100,
  SR = 0b101,
  OR = 0b110,
  AND = 0b111,

  FENCE = 0b000,
  FENCEI = 0b001,

  CSRRW = 0b001,
  CSRRS = 0b010,
  CSRRC = 0b011,
  CSRRWI = 0b101,
  CSRRSI = 0b110,
  CSRRCI = 0b111,

  MUL = 0b000,
  MULH = 0b001,
  MULHSU = 0b010,
  MULHU = 0b011,
  DIV = 0b100,
  DIVU = 0b101,
  REM = 0b110,
  REMU = 0b111,

  MULW = 0b000,
  DIVW = 0b100,
  DIVUW = 0b101,
  REMW = 0b110,
  REMUW = 0b111,

  WIDTH32 = 0b010,
  WIDTH64 = 0b011,

  S = 0b010,
  D = 0b011,
  J = 0b000,
  JN = 0b001,
  JX = 0b010,
  FMIN = 0b000,
  FMAX = 0b001,
  FEQ = 0b010,
  FLT = 0b001,
  FLE = 0b000,

  SH1ADD = 0b010,
  SH2ADD = 0b100,
  SH3ADD = 0b110,

  F3_COUNT = 0b001,

  MAX = 0b110,
  MAXU = 0b111,
  MIN = 0b100,
  MINU = 0b101,
  CLMUL = 0b001,
  CLMULH = 0b011,
  CLMULR = 0b010,

  SEXT = 0b001,
  ZEXT = 0b100,

  ROL = 0b001,
  ROR = 0b101,

  BCLR = 0b001,
  BEXT = 0b101,
  F3_BINV = 0b001,
  F3_BSET = 0b001,
};

enum Funct7 {
  F7_0 = 0,
  SRA = 0b0100000,
  SUB = 0b0100000,
  MULDIV = 0b0000001,

  FADDS = 0b0000000,
  FSUBS = 0b0000100,
  FMULS = 0b0001000,
  FDIVS = 0b0001100,
  FSQRTS = 0b0101100,
  FSGNJS = 0b0010000,
  FMINMAXS = 0b0010100,
  FCMPS = 0b1010000,
  FCLASSS = 0b1110000,
  FCVTintS = 0b1100000,
  FCVTSint = 0b1101000,
  FMVXW = 0b1110000,
  FMVWX = 0b1111000,

  FADDD = 0b0000001,
  FSUBD = 0b0000101,
  FMULD = 0b0001001,
  FDIVD = 0b0001101,
  FSQRTD = 0b0101101,
  FSGNJD = 0b0010001,
  FMINMAXD = 0b0010101,
  FCVTS = 0b0100000,
  FCVTD = 0b0100001,
  FCMPD = 0b1010001,
  FCLASSD = 0b1110001,
  FCVTintD = 0b1100001,
  FCVTDint = 0b1101001,
  FMVXD = 0b1110001,
  FMVDX = 0b1111001,

  ADDUW = 0b0000100,
  SHADD = 0b0010000,
  SLLIUW = 0b0000100,
  COUNT = 0b0110000,
  MINMAXCLMUL = 0b0000101,
  ROTATE = 0b0110000,
  BCLRBEXT = 0b0100100,
  BINV = 0b0110100,
  BSET = 0b0010100,
};

enum Funct5 {
  LR = 0b00010,
  SC = 0b00011,
  AMOSWAP = 0b00001,
  AMOADD = 0b00000,
  AMOXOR = 0b00100,
  AMOAND = 0b01100,
  AMOOR = 0b01000,
  AMOMIN = 0b10000,
  AMOMAX = 0b10100,
  AMOMINU = 0b11000,
  AMOMAXU = 0b11100,
};

enum Funct2 {
  F2_S = 0b00,
  F2_D = 0b01,
};

enum RoundingMode {
  RNE = 0b000,  // Round to Nearest, ties to Even
  RTZ = 0b001,  // Round toward Zero
  RDN = 0b010,  // Round Down (toward negative infinity)
  RUP = 0b011,  // Round Up (toward positive infinity)
  RMM = 0b100,  // Round to nearest, ties to Max Magnitude
  DYN = 0b111,  // Dynamic rounding mode
};

enum FcvtRs2 {
  W = 0b00000,
  WU = 0b00001,
  L = 0b00010,
  LU = 0b00011,
};

enum FClass {
  kFClassNegInfinity = 1 << 0,
  kFClassNegNormal = 1 << 1,
  kFClassNegSubnormal = 1 << 2,
  kFClassNegZero = 1 << 3,
  kFClassPosZero = 1 << 4,
  kFClassPosSubnormal = 1 << 5,
  kFClassPosNormal = 1 << 6,
  kFClassPosInfinity = 1 << 7,
  kFClassSignallingNan = 1 << 8,
  kFClassQuietNan = 1 << 9,
};

enum HartEffects {
  kWrite = 1 << 0,
  kRead = 1 << 1,
  kOutput = 1 << 2,
  kInput = 1 << 3,
  kMemory = kWrite | kRead,
  kIO = kOutput | kInput,
  kAll = kMemory | kIO,
};

const intptr_t kReleaseShift = 25;
const intptr_t kAcquireShift = 26;

#define DEFINE_REG_ENCODING(type, name, shift)                                 \
  inline uint32_t Is##name(type r) { return r.encoding() < 32; }               \
  inline uint32_t Encode##name(type r) {                                       \
    ASSERT(Is##name(r));                                                       \
    return r.encoding() << shift;                                              \
  }                                                                            \
  inline type Decode##name(uint32_t encoding) {                                \
    return type((encoding >> shift) & 31);                                     \
  }

DEFINE_REG_ENCODING(Register, Rd, 7)
DEFINE_REG_ENCODING(Register, Rs1, 15)
DEFINE_REG_ENCODING(Register, Rs2, 20)
DEFINE_REG_ENCODING(FRegister, FRd, 7)
DEFINE_REG_ENCODING(FRegister, FRs1, 15)
DEFINE_REG_ENCODING(FRegister, FRs2, 20)
DEFINE_REG_ENCODING(FRegister, FRs3, 27)
#undef DEFINE_REG_ENCODING

#define DEFINE_FUNCT_ENCODING(type, name, shift, mask)                         \
  inline uint32_t Is##name(type f) { return (f & mask) == f; }                 \
  inline uint32_t Encode##name(type f) {                                       \
    ASSERT(Is##name(f));                                                       \
    return f << shift;                                                         \
  }                                                                            \
  inline type Decode##name(uint32_t encoding) {                                \
    return static_cast<type>((encoding >> shift) & mask);                      \
  }

DEFINE_FUNCT_ENCODING(Opcode, Opcode, 0, 0x7F)
DEFINE_FUNCT_ENCODING(Funct2, Funct2, 25, 0x3)
DEFINE_FUNCT_ENCODING(Funct3, Funct3, 12, 0x7)
DEFINE_FUNCT_ENCODING(Funct5, Funct5, 27, 0x1F)
DEFINE_FUNCT_ENCODING(Funct7, Funct7, 25, 0x7F)
DEFINE_FUNCT_ENCODING(Funct12, Funct12, 20, 0xFFF)
#if XLEN == 32
DEFINE_FUNCT_ENCODING(uint32_t, Shamt, 20, 0x1F)
#elif XLEN == 64
DEFINE_FUNCT_ENCODING(uint32_t, Shamt, 20, 0x3F)
#endif
DEFINE_FUNCT_ENCODING(RoundingMode, RoundingMode, 12, 0x7)
#undef DEFINE_FUNCT_ENCODING

inline bool IsBTypeImm(intptr_t imm) {
  return Utils::IsInt(12, imm) && Utils::IsAligned(imm, 2);
}
inline uint32_t EncodeBTypeImm(intptr_t imm) {
  ASSERT(IsBTypeImm(imm));
  uint32_t encoded = 0;
  encoded |= ((imm >> 12) & 0x1) << 31;
  encoded |= ((imm >> 5) & 0x3f) << 25;
  encoded |= ((imm >> 1) & 0xf) << 8;
  encoded |= ((imm >> 11) & 0x1) << 7;
  return encoded;
}
inline intptr_t DecodeBTypeImm(uint32_t encoded) {
  uint32_t imm = 0;
  imm |= (((encoded >> 31) & 0x1) << 12);
  imm |= (((encoded >> 25) & 0x3f) << 5);
  imm |= (((encoded >> 8) & 0xf) << 1);
  imm |= (((encoded >> 7) & 0x1) << 11);
  return SignExtend(12, imm);
}

inline bool IsJTypeImm(intptr_t imm) {
  return Utils::IsInt(20, imm) && Utils::IsAligned(imm, 2);
}
inline uint32_t EncodeJTypeImm(intptr_t imm) {
  ASSERT(IsJTypeImm(imm));
  uint32_t encoded = 0;
  encoded |= ((imm >> 20) & 0x1) << 31;
  encoded |= ((imm >> 1) & 0x3ff) << 21;
  encoded |= ((imm >> 11) & 0x1) << 20;
  encoded |= ((imm >> 12) & 0xff) << 12;
  return encoded;
}
inline intptr_t DecodeJTypeImm(uint32_t encoded) {
  uint32_t imm = 0;
  imm |= (((encoded >> 31) & 0x1) << 20);
  imm |= (((encoded >> 21) & 0x3ff) << 1);
  imm |= (((encoded >> 20) & 0x1) << 11);
  imm |= (((encoded >> 12) & 0xff) << 12);
  return SignExtend(20, imm);
}

inline bool IsITypeImm(intptr_t imm) {
  return Utils::IsInt(12, imm);
}
inline uint32_t EncodeITypeImm(intptr_t imm) {
  ASSERT(IsITypeImm(imm));
  return imm << 20;
}
inline intptr_t DecodeITypeImm(uint32_t encoded) {
  return SignExtend(12, encoded >> 20);
}

inline bool IsUTypeImm(intptr_t imm) {
  return Utils::IsInt(32, imm) && Utils::IsAligned(imm, 1 << 12);
}
inline uint32_t EncodeUTypeImm(intptr_t imm) {
  ASSERT(IsUTypeImm(imm));
  return imm;
}
inline intptr_t DecodeUTypeImm(uint32_t encoded) {
  return SignExtend(32, encoded & ~((1 << 12) - 1));
}

inline bool IsSTypeImm(intptr_t imm) {
  return Utils::IsInt(12, imm);
}
inline uint32_t EncodeSTypeImm(intptr_t imm) {
  ASSERT(IsSTypeImm(imm));
  uint32_t encoded = 0;
  encoded |= ((imm >> 5) & 0x7f) << 25;
  encoded |= ((imm >> 0) & 0x1f) << 7;
  return encoded;
}
inline intptr_t DecodeSTypeImm(uint32_t encoded) {
  uint32_t imm = 0;
  imm |= (((encoded >> 25) & 0x7f) << 5);
  imm |= (((encoded >> 7) & 0x1f) << 0);
  return SignExtend(12, imm);
}

inline bool IsCInstruction(uint16_t parcel) {
  return (parcel & 3) != 3;
}

class Instruction {
 public:
  explicit Instruction(uint32_t encoding) : encoding_(encoding) {}
  uint32_t encoding() const { return encoding_; }

  size_t length() const { return 4; }

  Opcode opcode() const { return DecodeOpcode(encoding_); }

  Register rd() const { return DecodeRd(encoding_); }
  Register rs1() const { return DecodeRs1(encoding_); }
  Register rs2() const { return DecodeRs2(encoding_); }

  FRegister frd() const { return DecodeFRd(encoding_); }
  FRegister frs1() const { return DecodeFRs1(encoding_); }
  FRegister frs2() const { return DecodeFRs2(encoding_); }
  FRegister frs3() const { return DecodeFRs3(encoding_); }

  Funct2 funct2() const { return DecodeFunct2(encoding_); }
  Funct3 funct3() const { return DecodeFunct3(encoding_); }
  Funct5 funct5() const { return DecodeFunct5(encoding_); }
  Funct7 funct7() const { return DecodeFunct7(encoding_); }
  Funct12 funct12() const { return DecodeFunct12(encoding_); }

  uint32_t shamt() const { return DecodeShamt(encoding_); }
  RoundingMode rounding() const { return DecodeRoundingMode(encoding_); }

  std::memory_order memory_order() const {
    bool acquire = ((encoding_ >> kAcquireShift) & 1) != 0;
    bool release = ((encoding_ >> kReleaseShift) & 1) != 0;
    if (acquire && release) return std::memory_order_acq_rel;
    if (acquire) return std::memory_order_acquire;
    if (release) return std::memory_order_release;
    return std::memory_order_relaxed;
  }

  intx_t itype_imm() const { return DecodeITypeImm(encoding_); }
  intx_t stype_imm() const { return DecodeSTypeImm(encoding_); }
  intx_t btype_imm() const { return DecodeBTypeImm(encoding_); }
  intx_t utype_imm() const { return DecodeUTypeImm(encoding_); }
  intx_t jtype_imm() const { return DecodeJTypeImm(encoding_); }

  uint32_t csr() const { return encoding_ >> 20; }
  uint32_t zimm() const { return rs1().encoding(); }

 private:
  const uint32_t encoding_;
};

#define DEFINE_REG_ENCODING(type, name, shift)                                 \
  inline uint32_t Is##name(type r) { return r.encoding() < 32; }               \
  inline uint32_t Encode##name(type r) {                                       \
    ASSERT(Is##name(r));                                                       \
    return r.encoding() << shift;                                              \
  }                                                                            \
  inline type Decode##name(uint32_t encoding) {                                \
    return type((encoding >> shift) & 31);                                     \
  }

#define DEFINE_REG_PRIME_ENCODING(type, name, shift)                           \
  inline uint32_t Is##name(type r) {                                           \
    return (r.encoding() >= 8) && (r.encoding() < 16);                         \
  }                                                                            \
  inline uint32_t Encode##name(type r) {                                       \
    ASSERT(Is##name(r));                                                       \
    return (r.encoding() & 7) << shift;                                        \
  }                                                                            \
  inline type Decode##name(uint32_t encoding) {                                \
    return type(((encoding >> shift) & 7) + 8);                                \
  }

DEFINE_REG_ENCODING(Register, CRd, 7)
DEFINE_REG_ENCODING(Register, CRs1, 7)
DEFINE_REG_ENCODING(Register, CRs2, 2)
DEFINE_REG_ENCODING(FRegister, CFRd, 7)
DEFINE_REG_ENCODING(FRegister, CFRs1, 7)
DEFINE_REG_ENCODING(FRegister, CFRs2, 2)
DEFINE_REG_PRIME_ENCODING(Register, CRdp, 2)
DEFINE_REG_PRIME_ENCODING(Register, CRs1p, 7)
DEFINE_REG_PRIME_ENCODING(Register, CRs2p, 2)
DEFINE_REG_PRIME_ENCODING(FRegister, CFRdp, 2)
DEFINE_REG_PRIME_ENCODING(FRegister, CFRs1p, 7)
DEFINE_REG_PRIME_ENCODING(FRegister, CFRs2p, 2)
#undef DEFINE_REG_ENCODING
#undef DEFINE_REG_PRIME_ENCODING

inline bool IsCSPLoad4Imm(intptr_t imm) {
  return Utils::IsUint(8, imm) && Utils::IsAligned(imm, 4);
}
inline uint32_t EncodeCSPLoad4Imm(intptr_t imm) {
  ASSERT(IsCSPLoad4Imm(imm));
  uint32_t encoding = 0;
  encoding |= ((imm >> 5) & 0x1) << 12;
  encoding |= ((imm >> 2) & 0x7) << 4;
  encoding |= ((imm >> 6) & 0x3) << 2;
  return encoding;
}
inline intx_t DecodeCSPLoad4Imm(uint32_t encoding) {
  uint32_t imm = 0;
  imm |= ((encoding >> 12) & 0x1) << 5;
  imm |= ((encoding >> 4) & 0x7) << 2;
  imm |= ((encoding >> 2) & 0x3) << 6;
  return imm;
}

inline bool IsCSPLoad8Imm(intptr_t imm) {
  return Utils::IsUint(9, imm) && Utils::IsAligned(imm, 8);
}
inline uint32_t EncodeCSPLoad8Imm(intptr_t imm) {
  ASSERT(IsCSPLoad8Imm(imm));
  uint32_t encoding = 0;
  encoding |= ((imm >> 5) & 0x1) << 12;
  encoding |= ((imm >> 3) & 0x3) << 5;
  encoding |= ((imm >> 6) & 0x7) << 2;
  return encoding;
}
inline intx_t DecodeCSPLoad8Imm(uint32_t encoding) {
  uint32_t imm = 0;
  imm |= ((encoding >> 12) & 0x1) << 5;
  imm |= ((encoding >> 5) & 0x3) << 3;
  imm |= ((encoding >> 2) & 0x7) << 6;
  return imm;
}

inline bool IsCSPStore4Imm(intptr_t imm) {
  return Utils::IsUint(8, imm) && Utils::IsAligned(imm, 4);
}
inline uint32_t EncodeCSPStore4Imm(intptr_t imm) {
  ASSERT(IsCSPStore4Imm(imm));
  uint32_t encoding = 0;
  encoding |= ((imm >> 2) & 0xF) << 9;
  encoding |= ((imm >> 6) & 0x3) << 7;
  return encoding;
}
inline intx_t DecodeCSPStore4Imm(uint32_t encoding) {
  uint32_t imm = 0;
  imm |= ((encoding >> 9) & 0xF) << 2;
  imm |= ((encoding >> 7) & 0x3) << 6;
  return imm;
}

inline bool IsCSPStore8Imm(intptr_t imm) {
  return Utils::IsUint(9, imm) && Utils::IsAligned(imm, 8);
}
inline uint32_t EncodeCSPStore8Imm(intptr_t imm) {
  ASSERT(IsCSPStore8Imm(imm));
  uint32_t encoding = 0;
  encoding |= ((imm >> 3) & 0x7) << 10;
  encoding |= ((imm >> 6) & 0x7) << 7;
  return encoding;
}
inline intx_t DecodeCSPStore8Imm(uint32_t encoding) {
  uint32_t imm = 0;
  imm |= ((encoding >> 10) & 0x7) << 3;
  imm |= ((encoding >> 7) & 0x7) << 6;
  return imm;
}

inline bool IsCMem4Imm(intptr_t imm) {
  return Utils::IsUint(7, imm) && Utils::IsAligned(imm, 4);
}
inline uint32_t EncodeCMem4Imm(intptr_t imm) {
  ASSERT(IsCMem4Imm(imm));
  uint32_t encoding = 0;
  encoding |= ((imm >> 3) & 0x7) << 10;
  encoding |= ((imm >> 2) & 0x1) << 6;
  encoding |= ((imm >> 6) & 0x1) << 5;
  return encoding;
}
inline intx_t DecodeCMem4Imm(uint32_t encoding) {
  uint32_t imm = 0;
  imm |= ((encoding >> 10) & 0x7) << 3;
  imm |= ((encoding >> 6) & 0x1) << 2;
  imm |= ((encoding >> 5) & 0x1) << 6;
  return imm;
}

inline bool IsCMem8Imm(intptr_t imm) {
  return Utils::IsUint(8, imm) && Utils::IsAligned(imm, 8);
}
inline uint32_t EncodeCMem8Imm(intptr_t imm) {
  ASSERT(IsCMem8Imm(imm));
  uint32_t encoding = 0;
  encoding |= ((imm >> 3) & 0x7) << 10;
  encoding |= ((imm >> 6) & 0x3) << 5;
  return encoding;
}
inline intx_t DecodeCMem8Imm(uint32_t encoding) {
  uint32_t imm = 0;
  imm |= ((encoding >> 10) & 0x7) << 3;
  imm |= ((encoding >> 5) & 0x3) << 6;
  return imm;
}

inline bool IsCJImm(intptr_t imm) {
  return Utils::IsInt(11, imm) && Utils::IsAligned(imm, 2);
}
inline uint32_t EncodeCJImm(intptr_t imm) {
  ASSERT(IsCJImm(imm));
  uint32_t encoding = 0;
  encoding |= ((imm >> 11) & 0x1) << 12;
  encoding |= ((imm >> 4) & 0x1) << 11;
  encoding |= ((imm >> 8) & 0x3) << 9;
  encoding |= ((imm >> 10) & 0x1) << 8;
  encoding |= ((imm >> 6) & 0x1) << 7;
  encoding |= ((imm >> 7) & 0x1) << 6;
  encoding |= ((imm >> 1) & 0x7) << 3;
  encoding |= ((imm >> 5) & 0x1) << 2;
  return encoding;
}
inline intx_t DecodeCJImm(uint32_t encoding) {
  uint32_t imm = 0;
  imm |= ((encoding >> 12) & 0x1) << 11;
  imm |= ((encoding >> 11) & 0x1) << 4;
  imm |= ((encoding >> 9) & 0x3) << 8;
  imm |= ((encoding >> 8) & 0x1) << 10;
  imm |= ((encoding >> 7) & 0x1) << 6;
  imm |= ((encoding >> 6) & 0x1) << 7;
  imm |= ((encoding >> 3) & 0x7) << 1;
  imm |= ((encoding >> 2) & 0x1) << 5;
  return SignExtend(11, imm);
}

inline bool IsCBImm(intptr_t imm) {
  return Utils::IsInt(8, imm) && Utils::IsAligned(imm, 2);
}
inline uint32_t EncodeCBImm(intptr_t imm) {
  ASSERT(IsCBImm(imm));
  uint32_t encoding = 0;
  encoding |= ((imm >> 8) & 0x1) << 12;
  encoding |= ((imm >> 3) & 0x3) << 10;
  encoding |= ((imm >> 6) & 0x3) << 5;
  encoding |= ((imm >> 1) & 0x3) << 3;
  encoding |= ((imm >> 5) & 0x1) << 2;
  return encoding;
}
inline intx_t DecodeCBImm(uint32_t encoding) {
  uint32_t imm = 0;
  imm |= ((encoding >> 12) & 0x1) << 8;
  imm |= ((encoding >> 10) & 0x3) << 3;
  imm |= ((encoding >> 5) & 0x3) << 6;
  imm |= ((encoding >> 3) & 0x3) << 1;
  imm |= ((encoding >> 2) & 0x1) << 5;
  return SignExtend(8, imm);
}

inline bool IsCIImm(intptr_t imm) {
  return Utils::IsInt(6, imm) && Utils::IsAligned(imm, 1);
}
inline uint32_t EncodeCIImm(intptr_t imm) {
  ASSERT(IsCIImm(imm));
  uint32_t encoding = 0;
  encoding |= ((imm >> 5) & 0x1) << 12;
  encoding |= ((imm >> 0) & 0x1F) << 2;
  return encoding;
}
inline intx_t DecodeCIImm(uint32_t encoding) {
  uint32_t imm = 0;
  imm |= ((encoding >> 12) & 0x1) << 5;
  imm |= ((encoding >> 2) & 0x1F) << 0;
  return SignExtend(6, imm);
}

inline bool IsCUImm(intptr_t imm) {
  return Utils::IsInt(17, imm) && Utils::IsAligned(imm, 1 << 12);
}
inline uint32_t EncodeCUImm(intptr_t imm) {
  ASSERT(IsCUImm(imm));
  uint32_t encoding = 0;
  encoding |= ((imm >> 17) & 0x1) << 12;
  encoding |= ((imm >> 12) & 0x1F) << 2;
  return encoding;
}
inline intx_t DecodeCUImm(uint32_t encoding) {
  uint32_t imm = 0;
  imm |= ((encoding >> 12) & 0x1) << 17;
  imm |= ((encoding >> 2) & 0x1F) << 12;
  return SignExtend(17, imm);
}

inline bool IsCI16Imm(intptr_t imm) {
  return Utils::IsInt(10, imm) && Utils::IsAligned(imm, 16);
}
inline uint32_t EncodeCI16Imm(intptr_t imm) {
  ASSERT(IsCI16Imm(imm));
  uint32_t encoding = 0;
  encoding |= ((imm >> 9) & 0x1) << 12;
  encoding |= ((imm >> 4) & 0x1) << 6;
  encoding |= ((imm >> 6) & 0x1) << 5;
  encoding |= ((imm >> 7) & 0x3) << 3;
  encoding |= ((imm >> 5) & 0x1) << 2;
  return encoding;
}
inline intx_t DecodeCI16Imm(uint32_t encoding) {
  uint32_t imm = 0;
  imm |= ((encoding >> 12) & 0x1) << 9;
  imm |= ((encoding >> 6) & 0x1) << 4;
  imm |= ((encoding >> 5) & 0x1) << 6;
  imm |= ((encoding >> 3) & 0x3) << 7;
  imm |= ((encoding >> 2) & 0x1) << 5;
  return SignExtend(10, imm);
}

inline bool IsCI4SPNImm(intptr_t imm) {
  return Utils::IsUint(9, imm) && Utils::IsAligned(imm, 4);
}
inline uint32_t EncodeCI4SPNImm(intptr_t imm) {
  ASSERT(IsCI4SPNImm(imm));
  uint32_t encoding = 0;
  encoding |= ((imm >> 4) & 0x3) << 11;
  encoding |= ((imm >> 6) & 0xF) << 7;
  encoding |= ((imm >> 2) & 0x1) << 6;
  encoding |= ((imm >> 3) & 0x1) << 5;
  return encoding;
}
inline intx_t DecodeCI4SPNImm(uint32_t encoding) {
  uint32_t imm = 0;
  imm |= ((encoding >> 11) & 0x3) << 4;
  imm |= ((encoding >> 7) & 0xF) << 6;
  imm |= ((encoding >> 6) & 0x1) << 2;
  imm |= ((encoding >> 5) & 0x1) << 3;
  return imm;
}

enum COpcode {
  C_OP_MASK = 0b1110000000000011,

  C_ADDI4SPN = 0b0000000000000000,
  C_FLD = 0b0010000000000000,
  C_LW = 0b0100000000000000,
  C_FLW = 0b0110000000000000,
  C_LD = 0b0110000000000000,
  C_FSD = 0b1010000000000000,
  C_SW = 0b1100000000000000,
  C_FSW = 0b1110000000000000,
  C_SD = 0b1110000000000000,

  C_ADDI = 0b0000000000000001,
  C_JAL = 0b0010000000000001,
  C_ADDIW = 0b0010000000000001,
  C_LI = 0b0100000000000001,
  C_ADDI16SP = 0b0110000000000001,
  C_LUI = 0b0110000000000001,

  C_MISCALU = 0b1000000000000001,
  C_MISCALU_MASK = 0b1110110000000011,
  C_SRLI = 0b1000000000000001,
  C_SRAI = 0b1000010000000001,
  C_ANDI = 0b1000100000000001,
  C_RR = 0b1000110000000001,
  C_RR_MASK = 0b1111110001100011,
  C_SUB = 0b1000110000000001,
  C_XOR = 0b1000110000100001,
  C_OR = 0b1000110001000001,
  C_AND = 0b1000110001100001,
  C_SUBW = 0b1001110000000001,
  C_ADDW = 0b1001110000100001,

  C_J = 0b1010000000000001,
  C_BEQZ = 0b1100000000000001,
  C_BNEZ = 0b1110000000000001,

  C_SLLI = 0b0000000000000010,
  C_FLDSP = 0b0010000000000010,
  C_LWSP = 0b0100000000000010,
  C_FLWSP = 0b0110000000000010,
  C_LDSP = 0b0110000000000010,
  C_JR = 0b1000000000000010,
  C_MV = 0b1000000000000010,
  C_JALR = 0b1001000000000010,
  C_ADD = 0b1001000000000010,
  C_FSDSP = 0b1010000000000010,
  C_SWSP = 0b1100000000000010,
  C_FSWSP = 0b1110000000000010,
  C_SDSP = 0b1110000000000010,

  C_NOP = 0b0000000000000001,
  C_EBREAK = 0b1001000000000010,
};

class CInstruction {
 public:
  explicit CInstruction(uint16_t encoding) : encoding_(encoding) {}
  uint16_t encoding() const { return encoding_; }

  size_t length() const { return 2; }

  COpcode opcode() const { return COpcode(encoding_ & C_OP_MASK); }

  Register rd() const { return DecodeCRd(encoding_); }
  Register rs1() const { return DecodeCRd(encoding_); }
  Register rs2() const { return DecodeCRs2(encoding_); }
  Register rdp() const { return DecodeCRdp(encoding_); }
  Register rs1p() const { return DecodeCRs1p(encoding_); }
  Register rs2p() const { return DecodeCRs2p(encoding_); }
  FRegister frd() const { return DecodeCFRd(encoding_); }
  FRegister frs1() const { return DecodeCFRd(encoding_); }
  FRegister frs2() const { return DecodeCFRs2(encoding_); }
  FRegister frdp() const { return DecodeCFRdp(encoding_); }
  FRegister frs1p() const { return DecodeCFRs1p(encoding_); }
  FRegister frs2p() const { return DecodeCFRs2p(encoding_); }

  intx_t spload4_imm() { return DecodeCSPLoad4Imm(encoding_); }
  intx_t spload8_imm() { return DecodeCSPLoad8Imm(encoding_); }
  intx_t spstore4_imm() { return DecodeCSPStore4Imm(encoding_); }
  intx_t spstore8_imm() { return DecodeCSPStore8Imm(encoding_); }
  intx_t mem4_imm() { return DecodeCMem4Imm(encoding_); }
  intx_t mem8_imm() { return DecodeCMem8Imm(encoding_); }
  intx_t j_imm() { return DecodeCJImm(encoding_); }
  intx_t b_imm() { return DecodeCBImm(encoding_); }
  intx_t i_imm() { return DecodeCIImm(encoding_); }
  intx_t u_imm() { return DecodeCUImm(encoding_); }
  intx_t i16_imm() { return DecodeCI16Imm(encoding_); }
  intx_t i4spn_imm() { return DecodeCI4SPNImm(encoding_); }

 private:
  const uint16_t encoding_;
};

DEFINE_TYPED_ENUM_SET(Extension, uint32_t)
static constexpr Extension RV_I(0);  // Integer base
static constexpr Extension RV_M(1);  // Multiply/divide
static constexpr Extension RV_A(2);  // Atomic
static constexpr Extension RV_F(3);  // Single-precision floating point
static constexpr Extension RV_D(4);  // Double-precision floating point
static constexpr Extension RV_C(5);  // Compressed instructions
static constexpr ExtensionSet RV_G = RV_I | RV_M | RV_A | RV_F | RV_D;
static constexpr ExtensionSet RV_GC = RV_G | RV_C;
static constexpr Extension RV_Zba(6);  // Address generation
static constexpr Extension RV_Zbb(7);  // Basic bit-manipulation
static constexpr Extension RV_Zbc(8);  // Carry-less multiplicatio
static constexpr Extension RV_Zbs(9);  // Single-bit instructions
static constexpr ExtensionSet RV_B = RV_Zba | RV_Zbb | RV_Zbc | RV_Zbs;
static constexpr ExtensionSet RV_GCB = RV_GC | RV_B;

}  // namespace arancini::output::dynamic::riscv64

#endif  // VM_CONSTANTS_RISCV_H_
