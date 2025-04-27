#pragma once

#include <arancini/output/dynamic/machine-code-writer.h>
#include <fadec-enc.h>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace arancini::output::dynamic::x86 {

enum class x86_register_names {
    NONE = FE_NOREG,
    AX = FE_AX,
    CX = FE_CX,
    DX = FE_DX,
    BX = FE_BX,
    BP = FE_BP,
    SP = FE_SP,
    DI = FE_DI,
    SI = FE_SI,
    R8 = FE_R8,
    R9 = FE_R9,
    R10 = FE_R10,
    R11 = FE_R11,
    R12 = FE_R12,
    R13 = FE_R13,
    R14 = FE_R14,
    R15 = FE_R15,
    FS = FE_FS,
    GS = FE_GS,
};

struct x86_physical_register_operand {
    x86_register_names regname;

    x86_physical_register_operand(x86_register_names r) : regname(r) {}
};

struct x86_virtual_register_operand {
    unsigned int index;

    x86_virtual_register_operand(unsigned int i) : index(i) {}
};

struct x86_memory_operand {
    bool virt_base;

    union {
        x86_register_names pbase;
        unsigned int vbase;
    };

    x86_register_names index;
    x86_register_names seg;
    int scale;
    int displacement;

    x86_memory_operand(x86_register_names b)
        : virt_base(false), pbase(b), index(x86_register_names::NONE),
          seg(x86_register_names::NONE), scale(0), displacement(0) {}

    x86_memory_operand(x86_register_names b, int d)
        : virt_base(false), pbase(b), index(x86_register_names::NONE),
          seg(x86_register_names::NONE), scale(0), displacement(d) {}

    x86_memory_operand(unsigned int virt_base_index, int d)
        : virt_base(true), vbase(virt_base_index),
          index(x86_register_names::NONE), seg(x86_register_names::NONE),
          scale(0), displacement(d) {}

    x86_memory_operand(unsigned int virt_base_index, int d,
                       x86_register_names seg)
        : virt_base(true), vbase(virt_base_index),
          index(x86_register_names::NONE), seg(seg), scale(0), displacement(d) {
    }
};

struct x86_immediate_operand {
    union {
        unsigned char u8;
        unsigned short u16;
        unsigned int u32;
        unsigned long int u64;
        signed char s8;
        signed short s16;
        signed int s32;
        signed long int s64;
    };

    x86_immediate_operand(unsigned long v) : u64(v) {}
};

enum class x86_operand_type { invalid, preg, vreg, mem, imm };

struct x86_operand {
    x86_operand_type type;
    int width;
    union {
        x86_physical_register_operand pregop;
        x86_virtual_register_operand vregop;
        x86_memory_operand memop;
        x86_immediate_operand immop;
    };

    bool use, def;

    x86_operand()
        : type(x86_operand_type::invalid), width(0), use(false), def(false) {}

    x86_operand(const x86_physical_register_operand &o, int w)
        : type(x86_operand_type::preg), width(w), pregop(o), use(false),
          def(false) {}

    x86_operand(const x86_virtual_register_operand &o, int w)
        : type(x86_operand_type::vreg), width(w), vregop(o), use(false),
          def(false) {}

    x86_operand(const x86_memory_operand &o, int w)
        : type(x86_operand_type::mem), width(w), memop(o), use(false),
          def(false) {}

    x86_operand(const x86_immediate_operand &o, int w)
        : type(x86_operand_type::imm), width(w), immop(o), use(false),
          def(false) {}

    bool is_preg() const { return type == x86_operand_type::preg; }
    bool is_vreg() const { return type == x86_operand_type::vreg; }
    bool is_mem() const { return type == x86_operand_type::mem; }
    bool is_imm() const { return type == x86_operand_type::imm; }

    bool is_use() const { return use; }
    bool is_def() const { return def; }
    bool is_usedef() const { return use && def; }

    static x86_register_names assign(int index) {
        switch (index) {
        case 0:
            return x86_register_names::AX;
        case 1:
            return x86_register_names::CX;
        case 2:
            return x86_register_names::DX;
        case 3:
            return x86_register_names::BX;
        case 4:
            return x86_register_names::DI;
        case 5:
            return x86_register_names::SI;
        default:
            throw std::runtime_error("allocation assignment '" +
                                     std::to_string(index) + "' out-of-range");
        }
    }

    void allocate(int index) {
        if (type != x86_operand_type::vreg) {
            throw std::runtime_error("trying to allocate non-vreg");
        }

        type = x86_operand_type::preg;
        pregop.regname = assign(index);
    }

    void allocate_base(int index) {
        if (type != x86_operand_type::mem) {
            throw std::runtime_error("trying to allocate non-mem");
        }

        if (!memop.virt_base) {
            throw std::runtime_error("trying to allocate non-virtual membase ");
        }

        memop.virt_base = false;
        memop.pbase = assign(index);
    }

    void dump(std::ostream &os) const;
};

#define OPFORM_BITS(T, W) (((T & 3) << 4) | ((W / 8) & 15))
#define GET_OPFORM1(T1, W1) OPFORM_BITS(T1, W1)
#define GET_OPFORM2(T1, W1, T2, W2)                                            \
    ((OPFORM_BITS(T2, W2) << 6) | OPFORM_BITS(T1, W1))
#define GET_OPFORM3(T1, W1, T2, W2, T3, W3)                                    \
    ((OPFORM_BITS(T3, W3) << 12) | (OPFORM_BITS(T2, W2) << 6) |                \
     OPFORM_BITS(T1, W1))

#define R 1
#define M 2
#define I 3
#define DEFINE_OPFORM1(T1, W1) OF_##T1##W1 = GET_OPFORM1(T1, W1)
#define DEFINE_OPFORM2(T1, W1, T2, W2)                                         \
    OF_##T1##W1##_##T2##W2 = GET_OPFORM2(T1, W1, T2, W2)
#define DEFINE_OPFORM3(T1, W1, T2, W2, T3, W3)                                 \
    OF_##T1##W1##_##T2##W2##_##T3##W3 = GET_OPFORM3(T1, W1, T2, W2, T3, W3)

enum class x86_opform {
    OF_NONE = 0,
    DEFINE_OPFORM1(R, 8),
    DEFINE_OPFORM1(M, 8),
    DEFINE_OPFORM1(I, 8),
    DEFINE_OPFORM1(R, 16),
    DEFINE_OPFORM1(M, 16),
    DEFINE_OPFORM1(I, 16),
    DEFINE_OPFORM1(R, 32),
    DEFINE_OPFORM1(M, 32),
    DEFINE_OPFORM1(I, 32),
    DEFINE_OPFORM1(R, 64),
    DEFINE_OPFORM1(M, 64),
    DEFINE_OPFORM1(I, 64),
    DEFINE_OPFORM2(R, 8, R, 8),
    DEFINE_OPFORM2(R, 16, R, 16),
    DEFINE_OPFORM2(R, 32, R, 32),
    DEFINE_OPFORM2(R, 64, R, 64),
    DEFINE_OPFORM2(R, 8, M, 8),
    DEFINE_OPFORM2(R, 16, M, 16),
    DEFINE_OPFORM2(R, 32, M, 32),
    DEFINE_OPFORM2(R, 64, M, 64),
    DEFINE_OPFORM2(M, 8, R, 8),
    DEFINE_OPFORM2(M, 16, R, 16),
    DEFINE_OPFORM2(M, 32, R, 32),
    DEFINE_OPFORM2(M, 64, R, 64),
    DEFINE_OPFORM2(R, 8, I, 8),
    DEFINE_OPFORM2(R, 16, I, 16),
    DEFINE_OPFORM2(R, 32, I, 32),
    DEFINE_OPFORM2(R, 64, I, 64),
    DEFINE_OPFORM2(M, 8, I, 8),
    DEFINE_OPFORM2(M, 16, I, 16),
    DEFINE_OPFORM2(M, 32, I, 32),
    DEFINE_OPFORM2(M, 64, I, 64),

    DEFINE_OPFORM2(R, 16, R, 8),
    DEFINE_OPFORM2(R, 32, R, 8),
    DEFINE_OPFORM2(R, 64, R, 8),
    DEFINE_OPFORM2(R, 32, R, 16),
    DEFINE_OPFORM2(R, 64, R, 16),
    DEFINE_OPFORM2(R, 64, R, 32),
    DEFINE_OPFORM2(R, 16, M, 8),
    DEFINE_OPFORM2(R, 32, M, 8),
    DEFINE_OPFORM2(R, 64, M, 8),
    DEFINE_OPFORM2(R, 32, M, 16),
    DEFINE_OPFORM2(R, 64, M, 16),
    DEFINE_OPFORM2(R, 64, M, 32),

    DEFINE_OPFORM3(R, 64, R, 64, I, 64)
};

#undef R
#undef M
#undef I

struct x86_instruction {
    static const int nr_operands = 4;

    unsigned long raw_opcode;
    x86_opform opform;
    x86_operand operands[nr_operands];

    x86_instruction(unsigned long opc)
        : raw_opcode(opc), opform(x86_opform::OF_NONE) {}

    x86_instruction(unsigned long opc, const x86_operand &o1)
        : raw_opcode(opc), opform(classify(o1)) {
        operands[0] = o1;
    }

    x86_instruction(unsigned long opc, const x86_operand &o1,
                    const x86_operand &o2)
        : raw_opcode(opc), opform(classify(o1, o2)) {
        operands[0] = o1;
        operands[1] = o2;
    }

    x86_instruction(unsigned long opc, const x86_operand &o1,
                    const x86_operand &o2, const x86_operand &o3)
        : raw_opcode(opc), opform(classify(o1, o2, o3)) {
        operands[0] = o1;
        operands[1] = o2;
        operands[2] = o3;
    }

    static int operand_type_to_form_type(x86_operand_type t) {
        switch (t) {
        case x86_operand_type::preg:
        case x86_operand_type::vreg:
            return 1;
        case x86_operand_type::mem:
            return 2;
        case x86_operand_type::imm:
            return 3;

        default:
            return 0;
        }
    }

    static x86_opform classify(const x86_operand &o1) {
        return (x86_opform)GET_OPFORM1(operand_type_to_form_type(o1.type),
                                       o1.width);
    }

    static x86_opform classify(const x86_operand &o1, const x86_operand &o2) {
        return (x86_opform)GET_OPFORM2(
            operand_type_to_form_type(o1.type), o1.width,
            operand_type_to_form_type(o2.type), o2.width);
    }

    static x86_opform classify(const x86_operand &o1, const x86_operand &o2,
                               const x86_operand &o3) {
        return (x86_opform)GET_OPFORM3(
            operand_type_to_form_type(o1.type), o1.width,
            operand_type_to_form_type(o2.type), o2.width,
            operand_type_to_form_type(o3.type), o3.width);
    }

    static std::string opform_to_string(x86_opform of) {
        std::stringstream ss;

        unsigned int raw_of = (unsigned int)of;

        for (int i = 0; i < 4; i++) {
            int off = i * 6;
            int width = (raw_of >> off) & 0xf;
            int type = (raw_of >> (off + 4)) & 0x3;

            switch (type) {
            case 0:
                continue;

            case 1:
                ss << "R";
                break;
            case 2:
                ss << "M";
                break;
            case 3:
                ss << "I";
                break;
            }

            ss << std::dec << (width * 8);
        }

        return ss.str();
    }

    static x86_operand def(const x86_operand &o) {
        x86_operand r = o;
        r.def = true;
        return r;
    }

    static x86_operand use(const x86_operand &o) {
        x86_operand r = o;
        r.use = true;
        return r;
    }

    static x86_operand usedef(const x86_operand &o) {
        x86_operand r = o;
        r.use = true;
        r.def = true;
        return r;
    }

#define DEFINE_STDOP_T(name, mnemonic, DST, SRC)                               \
    static x86_instruction name(const x86_operand &dst,                        \
                                const x86_operand &src) {                      \
        switch (classify(dst, src)) {                                          \
        case x86_opform::OF_R8_R8:                                             \
            return x86_instruction(FE_##mnemonic##8rr, DST(dst), SRC(src));    \
        case x86_opform::OF_R8_M8:                                             \
            return x86_instruction(FE_##mnemonic##8rm, DST(dst), SRC(src));    \
        case x86_opform::OF_M8_R8:                                             \
            return x86_instruction(FE_##mnemonic##8mr, DST(dst), SRC(src));    \
        case x86_opform::OF_R8_I8:                                             \
            return x86_instruction(FE_##mnemonic##8ri, DST(dst), SRC(src));    \
        case x86_opform::OF_M8_I8:                                             \
            return x86_instruction(FE_##mnemonic##8mi, DST(dst), SRC(src));    \
                                                                               \
        case x86_opform::OF_R16_R16:                                           \
            return x86_instruction(FE_##mnemonic##16rr, DST(dst), SRC(src));   \
        case x86_opform::OF_R16_M16:                                           \
            return x86_instruction(FE_##mnemonic##16rm, DST(dst), SRC(src));   \
        case x86_opform::OF_M16_R16:                                           \
            return x86_instruction(FE_##mnemonic##16mr, DST(dst), SRC(src));   \
        case x86_opform::OF_R16_I16:                                           \
            return x86_instruction(FE_##mnemonic##16ri, DST(dst), SRC(src));   \
        case x86_opform::OF_M16_I16:                                           \
            return x86_instruction(FE_##mnemonic##16mi, DST(dst), SRC(src));   \
                                                                               \
        case x86_opform::OF_R32_R32:                                           \
            return x86_instruction(FE_##mnemonic##32rr, DST(dst), SRC(src));   \
        case x86_opform::OF_R32_M32:                                           \
            return x86_instruction(FE_##mnemonic##32rm, DST(dst), SRC(src));   \
        case x86_opform::OF_M32_R32:                                           \
            return x86_instruction(FE_##mnemonic##32mr, DST(dst), SRC(src));   \
        case x86_opform::OF_R32_I32:                                           \
            return x86_instruction(FE_##mnemonic##32ri, DST(dst), SRC(src));   \
        case x86_opform::OF_M32_I32:                                           \
            return x86_instruction(FE_##mnemonic##32mi, DST(dst), SRC(src));   \
                                                                               \
        case x86_opform::OF_R64_R64:                                           \
            return x86_instruction(FE_##mnemonic##64rr, DST(dst), SRC(src));   \
        case x86_opform::OF_R64_M64:                                           \
            return x86_instruction(FE_##mnemonic##64rm, DST(dst), SRC(src));   \
        case x86_opform::OF_M64_R64:                                           \
            return x86_instruction(FE_##mnemonic##64mr, DST(dst), SRC(src));   \
        case x86_opform::OF_R64_I64:                                           \
            return x86_instruction(FE_##mnemonic##64ri, DST(dst), SRC(src));   \
        case x86_opform::OF_M64_I64:                                           \
            return x86_instruction(FE_##mnemonic##64mi, DST(dst), SRC(src));   \
                                                                               \
        default:                                                               \
            throw std::runtime_error("unsupported encoding for mov");          \
        }                                                                      \
    }

#define DEFINE_STDOP(name, mnemonic) DEFINE_STDOP_T(name, mnemonic, usedef, use)

    DEFINE_STDOP_T(mov, MOV, def, use)
    DEFINE_STDOP(xor_, XOR)
    DEFINE_STDOP(and_, AND)
    DEFINE_STDOP(or_, OR)

    DEFINE_STDOP(add, ADD)
    DEFINE_STDOP(sub, SUB)

    static x86_instruction mul(const x86_operand &dst, const x86_operand &src) {
        switch (classify(dst, src)) {
        case x86_opform::OF_R64_R64:
            return x86_instruction(FE_IMUL64rr, usedef(dst), use(src));
        case x86_opform::OF_R64_I64:
            return x86_instruction(FE_IMUL64rri, def(dst), use(dst), use(src));
        default:
            throw std::runtime_error("unsupported encoding '" +
                                     opform_to_string(classify(dst, src)) +
                                     "' for mul");
        }
    }

    static x86_instruction movz(const x86_operand &dst,
                                const x86_operand &src) {
        switch (classify(dst, src)) {
        case x86_opform::OF_R16_R8:
            return x86_instruction(FE_MOVZXr16r8, def(dst), use(src));
        case x86_opform::OF_R16_M8:
            return x86_instruction(FE_MOVZXr16m8, def(dst), use(src));
        case x86_opform::OF_R32_R8:
            return x86_instruction(FE_MOVZXr32r8, def(dst), use(src));
        case x86_opform::OF_R32_M8:
            return x86_instruction(FE_MOVZXr32m8, def(dst), use(src));
        case x86_opform::OF_R64_R8:
            return x86_instruction(FE_MOVZXr64r8, def(dst), use(src));
        case x86_opform::OF_R64_M8:
            return x86_instruction(FE_MOVZXr64m8, def(dst), use(src));
        case x86_opform::OF_R64_R32:
            return x86_instruction(FE_MOV32rr, def(dst), use(src));

        default:
            throw std::runtime_error("unsupported encoding '" +
                                     opform_to_string(classify(dst, src)) +
                                     "' for movz");
        }
    }

    static x86_instruction movs(const x86_operand &dst,
                                const x86_operand &src) {
        switch (classify(dst, src)) {
        case x86_opform::OF_R16_R8:
            return x86_instruction(FE_MOVSXr16r8, def(dst), use(src));
        case x86_opform::OF_R16_M8:
            return x86_instruction(FE_MOVSXr16m8, def(dst), use(src));
        case x86_opform::OF_R32_R8:
            return x86_instruction(FE_MOVSXr32r8, def(dst), use(src));
        case x86_opform::OF_R32_M8:
            return x86_instruction(FE_MOVSXr32m8, def(dst), use(src));
        case x86_opform::OF_R64_R8:
            return x86_instruction(FE_MOVSXr64r8, def(dst), use(src));
        case x86_opform::OF_R64_M8:
            return x86_instruction(FE_MOVSXr64m8, def(dst), use(src));
        case x86_opform::OF_R64_R32:
            return x86_instruction(FE_MOVSXr64r32, def(dst), use(src));
        case x86_opform::OF_R64_M32:
            return x86_instruction(FE_MOVSXr64m32, def(dst), use(src));
        default:
            throw std::runtime_error("unsupported encoding '" +
                                     opform_to_string(classify(dst, src)) +
                                     "' for movs");
        }
    }

#define DEFINE_SETOP(name, mnemonic)                                           \
    static x86_instruction name(const x86_operand &dst) {                      \
        switch (classify(dst)) {                                               \
        case x86_opform::OF_M8:                                                \
            return x86_instruction(FE_##mnemonic##8m, def(dst));               \
        case x86_opform::OF_R8:                                                \
            return x86_instruction(FE_##mnemonic##8r, def(dst));               \
        default:                                                               \
            throw std::runtime_error("invalid opform");                        \
        }                                                                      \
    }

    DEFINE_SETOP(setc, SETC)
    DEFINE_SETOP(seto, SETO)
    DEFINE_SETOP(setz, SETZ)
    DEFINE_SETOP(sets, SETS)

    static x86_instruction int3() { return x86_instruction(FE_INT3); }
    static x86_instruction ret() { return x86_instruction(FE_RET); }

    void dump(std::ostream &os) const;
    void emit(machine_code_writer &writer) const;
    void kill() { raw_opcode = 0; }

    bool is_dead() const { return raw_opcode == 0; }

    x86_operand &get_operand(int index) { return operands[index]; }
};
} // namespace arancini::output::dynamic::x86
