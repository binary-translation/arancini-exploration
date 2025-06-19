#include "arancini/ir/value-type.h"
#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>
#include <arancini/ir/port.h>
#include <arancini/util/logger.h>


using namespace arancini::ir;
using namespace arancini::input::x86::translators;

translation_result translator::translate(off_t address,
                                         xed_decoded_inst_t *xed_inst,
                                         const std::string &disasm) {
    switch (xed_decoded_inst_get_iclass(xed_inst)) {
        // TODO: this is a bad way of avoiding empty packets. Should be done by
        // checking that the translator is a nop_translator, not hardcoded
        // switch case
        // case XED_ICLASS_NOP:
        //	case XED_ICLASS_HLT:
    case XED_ICLASS_CPUID:
    case XED_ICLASS_PREFETCHNTA:
    case XED_ICLASS_PAUSE:
    case XED_ICLASS_NOP:
        builder_.begin_packet(address, disasm);
        return builder_.end_packet() == packet_type::end_of_block
                   ? translation_result::end_of_block
                   : translation_result::noop;
    default:
        builder_.begin_packet(address, disasm);

        xed_inst_ = xed_inst;
        do_translate();

        return builder_.end_packet() == packet_type::end_of_block
                   ? translation_result::end_of_block
                   : translation_result::normal;
    }
}

void translator::dump_xed_encoding(void) {
    xed_decoded_inst_t *xed_ins = xed_inst();
    const xed_inst_t *insn = xed_decoded_inst_inst(xed_ins);
    auto nops = xed_decoded_inst_noperands(xed_ins);
    char buf[64];

    xed_format_context(XED_SYNTAX_INTEL, xed_ins, buf, sizeof(buf) - 1, 0,
                       nullptr, 0);
    util::global_logger.info("Decoding: {}\n", buf);
    util::global_logger.info("XED encoding:\n");
    for (unsigned int opnum = 0; opnum < nops; opnum++) {
        auto operand = xed_inst_operand(insn, opnum);
        xed_operand_print(operand, buf, sizeof(buf) - 1);
        if (util::global_logger.get_level() <=
            util::global_logging::levels::info)
            util::global_logger.log("{} ", buf);
    }
    if (util::global_logger.get_level() <= util::global_logging::levels::info)
        util::global_logger.log("\n", buf);
}

reg_offsets translator::xedreg_to_offset(xed_reg_enum_t reg) {
    auto regclass = xed_reg_class(reg);

    switch (regclass) {
    case XED_REG_CLASS_GPR: {
        // FIXME AH, CH, DH, BH give wrong offset
        auto largest_reg = xed_get_largest_enclosing_register(reg);
        return (reg_offsets)((int)((largest_reg - XED_REG_RAX) * 8) +
                             (int)reg_offsets::RAX);
    }
    case XED_REG_CLASS_XMM:
    case XED_REG_CLASS_YMM:
    case XED_REG_CLASS_ZMM: {
        return (reg_offsets)((int)((reg - XED_REG_ZMM0) * 64) +
                             (int)reg_offsets::ZMM0);
    }
    default:
        throw std::runtime_error(
            "unsupported register class when computing offset from xed");
    }
}

action_node *translator::write_operand(int opnum, port &value) {
    const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst());
    auto operand = xed_inst_operand(insn, opnum);
    auto opname = xed_operand_name(operand);

    switch (opname) {
    case XED_OPERAND_REG0:
    case XED_OPERAND_REG1:
    case XED_OPERAND_REG2: {
        auto reg = xed_decoded_inst_get_reg(xed_inst(), opname);
        auto regclass = xed_reg_class(reg);

        switch (regclass) {
        case XED_REG_CLASS_GPR: {
            auto width = value.type().width();
            switch (width) {
            // Behaviour is extracted from the Intel® 64 and IA-32 Architectures
            // Software Developer’s Manual, Volume 1 Basic Architecture,
            // Section 3.4.1.1
            case 64: // e.g. RAX
                // value is bitcast to u64 and written
                if (value.type().type_class() ==
                    value_type_class::signed_integer) {
                    return write_reg(
                        xedreg_to_offset(reg),
                        builder_.insert_bitcast(value_type::u64(), value)
                            ->val());
                } else {
                    return write_reg(xedreg_to_offset(reg), value);
                }
            case 32: // e.g. EAX
                // x86_64 requires that the high 32bit are zeroed when writing
                // to 32bit version of registers
                return write_reg(
                    xedreg_to_offset(reg),
                    builder_.insert_zx(value_type::u64(), value)->val());
            case 16: { // e.g. AX
                // x86_64 requires that the upper bits [63..16] are untouched
                auto orig = read_reg(value_type::u64(), xedreg_to_offset(reg));
                auto res =
                    builder_.insert_bit_insert(orig->val(), value, 0, 16);
                return write_reg(xedreg_to_offset(reg), res->val());
            }
            case 8: { // e.g. AL/AH
                // x86_64 requires that the upper bits [63..16/8] are untouched
                auto orig = read_reg(value_type::u64(), xedreg_to_offset(reg));
                value_node *res;
                if (reg >= XED_REG_AL && reg <= XED_REG_R15B) { // lower 8 bits
                    res = builder_.insert_bit_insert(orig->val(), value, 0, 8);
                } else {                                        // bits [15..8]
                    res = builder_.insert_bit_insert(orig->val(), value, 8, 8);
                }
                return write_reg(xedreg_to_offset(reg), res->val());
            }
            default:
                throw std::runtime_error(
                    "" + std::string(__FILE__) + ":" +
                    std::to_string(__LINE__) +
                    ": unsupported general purpose register size: " +
                    std::to_string(width));
            }
        }

        case XED_REG_CLASS_XMM:
        case XED_REG_CLASS_YMM:
        case XED_REG_CLASS_ZMM: {
            auto enc_reg_off =
                xedreg_to_offset(xed_get_largest_enclosing_register(reg));
            value_node *orig;
            auto val_len = value.type().width();
            value_node *enc;
            if (!xed_classify_sse(xed_inst())) {
                value_node *flat;
                switch (val_len) {
                case 32:
                    flat = builder_.insert_bitcast(value_type::u32(), value);
                    break;
                case 64:
                    flat = builder_.insert_bitcast(value_type::u64(), value);
                    break;
                case 128:
                    flat = builder_.insert_bitcast(value_type::u128(), value);
                    break;
                case 256:
                    flat = builder_.insert_bitcast(value_type::u256(), value);
                    break;
                case 512:
                    flat = builder_.insert_bitcast(value_type::u512(), value);
                    break;
                default:
                    throw std::runtime_error("unsupported value width when "
                                             "writing X/Y/ZMM registers");
                }
                switch (xed_get_register_width_bits64(
                    xed_get_largest_enclosing_register(reg))) {
                case 128:
                    return write_reg(
                        enc_reg_off,
                        builder_.insert_zx(value_type::u128(), flat->val())
                            ->val());
                case 256:
                    return write_reg(
                        enc_reg_off,
                        builder_.insert_zx(value_type::u256(), flat->val())
                            ->val());
                case 512:
                    return write_reg(
                        enc_reg_off,
                        builder_.insert_zx(value_type::u512(), flat->val())
                            ->val());
                }
            }
            switch (xed_get_register_width_bits64(
                xed_get_largest_enclosing_register(reg))) {
            case 128:
                enc = read_reg(value_type::u128(), enc_reg_off);
                break;
            case 256:
                enc = read_reg(value_type::u256(), enc_reg_off);
                break;
            case 512:
                enc = read_reg(value_type::u512(), enc_reg_off);
                break;
            }
            auto enc_len = enc->val().type().width();
            switch (val_len) {
            case 32: {
                orig = builder_.insert_bitcast(value_type::u32(), value);
                enc = builder_.insert_bitcast(
                    value_type::vector(value_type::u32(), enc_len / val_len),
                    enc->val());
                enc = builder_.insert_vector_insert(enc->val(), 0, orig->val());
            } break;
            case 64: {
                orig = builder_.insert_bitcast(value_type::u64(), value);
                enc = builder_.insert_bitcast(
                    value_type::vector(value_type::u64(), enc_len / val_len),
                    enc->val());
                enc = builder_.insert_vector_insert(enc->val(), 0, orig->val());
            } break;
            case 128: {
                orig = builder_.insert_bitcast(value_type::u128(), value);
                enc = builder_.insert_bitcast(
                    value_type::vector(value_type::u128(), enc_len / val_len),
                    enc->val());
                enc = builder_.insert_vector_insert(enc->val(), 0, orig->val());
            } break;
            case 256: {
                orig = builder_.insert_bitcast(value_type::u256(), value);
                enc = builder_.insert_bitcast(
                    value_type::vector(value_type::u256(), enc_len / val_len),
                    enc->val());
                enc = builder_.insert_vector_insert(enc->val(), 0, orig->val());
            } break;
            case 512: {
                orig = builder_.insert_bitcast(value_type::u512(), value);
                enc = builder_.insert_bitcast(
                    value_type::vector(value_type::u512(), enc_len / val_len),
                    enc->val());
                enc = builder_.insert_vector_insert(enc->val(), 0, orig->val());
            } break;
            }
            return write_reg(enc_reg_off, enc->val());
        }

        case XED_REG_CLASS_X87: {
            switch (reg) {
                // TODO put the convert logic here?
            case XED_REG_ST0:
            case XED_REG_ST1:
            case XED_REG_ST2:
            case XED_REG_ST3:
            case XED_REG_ST4:
            case XED_REG_ST5:
            case XED_REG_ST6:
            case XED_REG_ST7: {
                // TODO: FPU: Check Flags
                auto st_idx = reg - XED_REG_ST0;
                return fpu_stack_set(st_idx, value);
            }
            default:
                throw std::runtime_error("unsupported x87 register type");
            }
            break;
        }

        default:
            throw std::runtime_error(
                "" + std::string(__FILE__) + ":" + std::to_string(__LINE__) +
                ": unsupported register class: " + std::to_string(regclass));
        }
    }

    case XED_OPERAND_MEM0: {
        auto address = compute_address(0);
        return builder_.insert_write_mem(address->val(), value);
    }

    default:
        throw std::logic_error("unsupported write operand type: " +
                               std::to_string((int)opname));
    }
}

value_node *translator::read_operand(int opnum) {
    const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst());
    auto operand = xed_inst_operand(insn, opnum);
    auto opname = xed_operand_name(operand);

    switch (opname) {
    case XED_OPERAND_REG0:
    case XED_OPERAND_REG1:
    case XED_OPERAND_REG2:
    case XED_OPERAND_REG3: {
        auto reg = xed_decoded_inst_get_reg(xed_inst(), opname);
        auto regclass = xed_reg_class(reg);

        switch (regclass) {
        case XED_REG_CLASS_GPR:
            switch (xed_get_register_width_bits(reg)) {
            case 8:
                // FIXME AH, CH, DH, BH
                return read_reg(value_type::u8(), xedreg_to_offset(reg));
            case 16:
                return read_reg(value_type::u16(), xedreg_to_offset(reg));
            case 32:
                return read_reg(value_type::u32(), xedreg_to_offset(reg));
            case 64:
                return read_reg(value_type::u64(), xedreg_to_offset(reg));
            default:
                throw std::runtime_error("unsupported register size");
            }

        case XED_REG_CLASS_XMM:
            return read_reg(
                value_type::u128(),
                xedreg_to_offset(xed_get_largest_enclosing_register(reg)));
        case XED_REG_CLASS_YMM:
            return read_reg(
                value_type::u256(),
                xedreg_to_offset(xed_get_largest_enclosing_register(reg)));
        case XED_REG_CLASS_ZMM:
            return read_reg(
                value_type::u512(),
                xedreg_to_offset(xed_get_largest_enclosing_register(reg)));

            // case XED_REG_CLASS_FLAGS:
            // 	return read_reg(value_type::u64(), xedreg_to_offset(reg));

        case XED_REG_CLASS_X87: {
            switch (reg) {
                // TODO put the convert logic here?
            case XED_REG_ST0:
            case XED_REG_ST1:
            case XED_REG_ST2:
            case XED_REG_ST3:
            case XED_REG_ST4:
            case XED_REG_ST5:
            case XED_REG_ST6:
            case XED_REG_ST7: {
                // TODO: FPU: Check Flags
                auto st_idx = reg - XED_REG_ST0;
                return fpu_stack_get(st_idx);
            }
            default:
                throw std::runtime_error("unsupported x87 register type");
            }
        }
        case XED_REG_CLASS_PSEUDOX87: {
            switch (reg) {
            case XED_REG_X87CONTROL:
                return read_reg(value_type::u16(), reg_offsets::X87_CTRL);
            case XED_REG_X87STATUS:
                return read_reg(value_type::u16(), reg_offsets::X87_STS);
            default:
                throw std::runtime_error("unsupported pseudoX87 register type");
            }
            break;
        }

        default:
            throw std::runtime_error(
                std::string(__FILE__) + ":" + std::to_string(__LINE__) +
                ": unsupported register class: " + std::to_string(regclass));
        }
    }

    case XED_OPERAND_IMM0: {
        if (xed_decoded_inst_get_immediate_is_signed(xed_inst())) {
            switch (xed_decoded_inst_get_immediate_width_bits(xed_inst())) {
            case 8:
                return builder_.insert_constant_s8(
                    xed_decoded_inst_get_signed_immediate(xed_inst()));
            case 16:
                return builder_.insert_constant_s16(
                    xed_decoded_inst_get_signed_immediate(xed_inst()));
            case 32:
                return builder_.insert_constant_s32(
                    xed_decoded_inst_get_signed_immediate(xed_inst()));
            case 64:
                return builder_.insert_constant_s64(
                    xed_decoded_inst_get_signed_immediate(xed_inst()));
            default:
                throw std::runtime_error("unsupported immediate width");
            }
        } else {
            switch (xed_decoded_inst_get_immediate_width_bits(xed_inst())) {
            case 8:
                return builder_.insert_constant_u8(
                    xed_decoded_inst_get_unsigned_immediate(xed_inst()));
            case 16:
                return builder_.insert_constant_u16(
                    xed_decoded_inst_get_unsigned_immediate(xed_inst()));
            case 32:
                return builder_.insert_constant_u32(
                    xed_decoded_inst_get_unsigned_immediate(xed_inst()));
            case 64:
                return builder_.insert_constant_u64(
                    xed_decoded_inst_get_unsigned_immediate(xed_inst()));
            default:
                throw std::runtime_error("unsupported immediate width");
            }
        }
    }

    case XED_OPERAND_MEM0:
    case XED_OPERAND_MEM1: {
        auto mem_idx = opname - XED_OPERAND_MEM0;
        auto addr = compute_address(mem_idx);

        switch (
            xed_decoded_inst_get_memory_operand_length(xed_inst(), mem_idx)) {
        case 1:
            return builder_.insert_read_mem(value_type::u8(), addr->val());
        case 2:
            return builder_.insert_read_mem(value_type::u16(), addr->val());
        case 4:
            return builder_.insert_read_mem(value_type::u32(), addr->val());
        case 8:
            return builder_.insert_read_mem(value_type::u64(), addr->val());
        case 10:
            return builder_.insert_read_mem(value_type::f80(), addr->val());
        case 16:
            return builder_.insert_read_mem(value_type::u128(), addr->val());

        default:
            throw std::runtime_error("invalid memory width in read");
        }
    }
    case XED_OPERAND_RELBR: {
        int32_t displacement =
            xed_decoded_inst_get_branch_displacement(xed_inst());
        return builder_.insert_constant_u64(displacement);
    }
    default:
        throw std::logic_error("unsupported read operand type: " +
                               std::to_string((int)opname));
    }
}

ssize_t translator::get_operand_width(int opnum) {
    const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst());
    auto operand = xed_inst_operand(insn, opnum);
    auto opname = xed_operand_name(operand);

    switch (opname) {
    case XED_OPERAND_REG0:
    case XED_OPERAND_REG1:
    case XED_OPERAND_REG2: {
        auto reg = xed_decoded_inst_get_reg(xed_inst(), opname);
        return xed_get_register_width_bits(reg);
    }

    case XED_OPERAND_IMM0:
        return xed_decoded_inst_get_immediate_width_bits(xed_inst());

    case XED_OPERAND_MEM0:
        return 8 * xed_decoded_inst_get_memory_operand_length(xed_inst(), 0);
    case XED_OPERAND_MEM1:
        return 8 * xed_decoded_inst_get_memory_operand_length(xed_inst(), 1);

    default:
        throw std::runtime_error("unsupported operand width query");
    }
}

bool translator::is_memory_operand(int opnum) {
    const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst());
    auto operand = xed_inst_operand(insn, opnum);
    auto opname = xed_operand_name(operand);

    return (opname == XED_OPERAND_MEM0 || opname == XED_OPERAND_MEM1);
}

bool translator::is_immediate_operand(int opnum) {
    const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst());
    auto operand = xed_inst_operand(insn, opnum);
    auto opname = xed_operand_name(operand);

    return (opname == XED_OPERAND_IMM0 || opname == XED_OPERAND_IMM1);
}

value_node *translator::compute_address(int mem_idx) {
    auto base_reg = xed_decoded_inst_get_base_reg(xed_inst(), mem_idx);
    auto displ = xed_decoded_inst_get_memory_displacement(xed_inst(), mem_idx);

    auto index = xed_decoded_inst_get_index_reg(xed_inst(), mem_idx);
    auto scale = xed_decoded_inst_get_scale(xed_inst(), mem_idx);

    auto i = scale == 1   ? 0
             : scale == 2 ? 1
             : scale == 4 ? 2
             : scale == 8 ? 3
                          : 0;

    auto seg = xed_decoded_inst_get_seg_reg(xed_inst(), mem_idx);

    if (xed_get_register_width_bits(base_reg) != 64 &&
        base_reg != XED_REG_INVALID) {
        throw std::runtime_error("base reg invalid size");
    }

    value_node *address_base{nullptr};

    if (base_reg != XED_REG_INVALID) {
        if (base_reg == XED_REG_RIP) {
            address_base = builder_.insert_read_pc();
            xed_uint_t instruction_length =
                xed_decoded_inst_get_length(xed_inst());
            address_base = builder_.insert_add(
                address_base->val(),
                builder_.insert_constant_u64(instruction_length)->val());
        } else {
            address_base =
                read_reg(value_type::u64(), xedreg_to_offset(base_reg));
        }
    }

    if (index != XED_REG_INVALID) {
        value_node *scaled_index;
        if (i != 0) {
            scaled_index = builder_.insert_lsl(
                read_reg(value_type::u64(), xedreg_to_offset(index))->val(),
                builder_.insert_constant_u64(i)->val());
        } else {
            scaled_index = read_reg(value_type::u64(), xedreg_to_offset(index));
        }

        if (!address_base) {
            address_base = scaled_index;
        } else {
            address_base =
                builder_.insert_add(address_base->val(), scaled_index->val());
        }
    }

    if (displ) {
        value_node *displ_node = builder_.insert_constant_u64(displ);
        if (!address_base) {
            address_base = displ_node;
        } else {
            address_base =
                builder_.insert_add(address_base->val(), displ_node->val());
        }
    }

    if (!address_base) {
        address_base = builder_.insert_constant_u64(0);
    }

    if (seg == XED_REG_FS) {
        address_base = builder_.insert_add(
            address_base->val(),
            read_reg(value_type::u64(), reg_offsets::FS)->val());
    } else if (seg == XED_REG_GS) {
        address_base = builder_.insert_add(
            address_base->val(),
            read_reg(value_type::u64(), reg_offsets::GS)->val());
    }

    return address_base;
}

value_node *translator::fpu_compute_stack_index(int stack_idx) {
    auto x87_status = read_reg(value_type::u16(), reg_offsets::X87_STS);
    // Get the TOP of the stack
    auto top = builder_.insert_zx(
        value_type::u64(),
        builder_.insert_bit_extract(x87_status->val(), 11, 3)->val());

    // If accessing ST(i) with i > 0, add the offset of the index to the address
    if (stack_idx) {
        auto idx_offset = builder_.insert_constant_u64(stack_idx);
        top = builder_.insert_add(top->val(), idx_offset->val());
        top = builder_.insert_mod(top->val(),
                                  builder_.insert_constant_u64(8)->val());
    }

    return top;
}

value_node *translator::fpu_compute_stack_addr(int stack_idx) {
    auto x87_stack_base =
        read_reg(value_type::u64(), reg_offsets::X87_STACK_BASE);

    // Get the TOP of the stack and multiply by 8 to get the proper offset (an
    // FPU stack register is 8-bytes wide, as we downgreade it to f64)
    auto top = fpu_compute_stack_index(stack_idx);
    top =
        builder_.insert_mul(top->val(), builder_.insert_constant_u64(8)->val());

    // Add the TOP offset to the base address of the stack
    auto addr = builder_.insert_add(x87_stack_base->val(), top->val());

    return addr;
}

value_node *translator::fpu_stack_get(int stack_idx) {
    auto st0_addr = fpu_compute_stack_addr(stack_idx);
    auto st0 = builder_.insert_read_mem(value_type::f64(), st0_addr->val());
    return builder().insert_bitcast(value_type::f64(), st0->val());
}

action_node *translator::fpu_stack_set(int stack_idx, port &val) {
    // Write the value to ST(stack_idx)
    auto st0_addr = fpu_compute_stack_addr(stack_idx);
    return builder_.insert_write_mem(st0_addr->val(), val);
}

value_node *translator::fpu_tag_get(int stack_idx) {
    auto x87_tag = read_reg(value_type::u16(), reg_offsets::X87_TAG);
    // Get the TOP of the stack
    auto top = fpu_compute_stack_index(stack_idx);

    // we shift our register by `2 * top` to have the requested index as the
    // first two bits
    auto res = builder_.insert_lsr(
        x87_tag->val(),
        builder_.insert_lsl(top->val(), builder_.insert_constant_u1(1)->val())
            ->val());

    // We AND to have the value remaining
    return builder_.insert_and(res->val(),
                               builder_.insert_constant_u16(0b11)->val());
}

action_node *translator::fpu_tag_set(int stack_idx, port &val) {
    auto x87_tag = read_reg(value_type::u16(), reg_offsets::X87_TAG);
    // Calculate shift amount (2 * top_of_stack)
    auto top = fpu_compute_stack_index(stack_idx);
    auto shift_amount =
        builder_.insert_lsl(top->val(), builder_.insert_constant_u1(1)->val());

    // Clear old tag (shift 0b11 into position, negate and apply)
    auto clear_mask = builder_.insert_not(
        builder_
            .insert_lsl(builder_.insert_constant_u16(0b11)->val(),
                        shift_amount->val())
            ->val());
    x87_tag = builder_.insert_and(x87_tag->val(), clear_mask->val());

    // Set new tag (shift val into position and apply)
    auto val_mask = builder_.insert_lsl(val, shift_amount->val());
    x87_tag = builder_.insert_or(x87_tag->val(), val_mask->val());
    return write_reg(reg_offsets::X87_TAG, x87_tag->val());
}

action_node *translator::fpu_stack_index_move(int val) {
    if (val != 1 /* pop */ && val != -1 /* push */) {
        throw std::logic_error("Cannot move the FPU stack by " +
                               std::to_string(val) + ". Must be 1 or -1.");
    }

    auto x87_status = read_reg(value_type::u16(), reg_offsets::X87_STS);
    auto top = builder_.insert_zx(
        value_type::u8(),
        builder_.insert_bit_extract(x87_status->val(), 11, 3)->val());

    // compute the new top index (safely)
    top =
        builder_.insert_add(top->val(), builder_.insert_constant_u8(8)->val());
    top = builder_.insert_add(
        top->val(), builder_.insert_constant_i(top->val().type(), val)->val());
    top =
        builder_.insert_mod(top->val(), builder_.insert_constant_u8(8)->val());

    // Write new status
    x87_status =
        builder_.insert_bit_insert(x87_status->val(), top->val(), 11, 3);
    return write_reg(reg_offsets::X87_STS, x87_status->val());
}

action_node *translator::fpu_push(port &val) {
    auto orig_val = builder_.insert_bitcast(value_type::f64(), val);
    // c1 is 1, iff overflow occured otherwise 0

    // Check for overflow (if 0b11 is set as tag for the new word, none occurs)
    auto old_tag_test = fpu_tag_get(7);
    // Compare by checking old_tag_test == 0b11
    auto overflow_check_val = builder_.insert_cmpeq(
        old_tag_test->val(), builder_.insert_constant_u16(0b11)->val());

    // No overflow block (default)
    // calculate and set correct TAG value (we ignore specials, e.g. NaN,
    // infinity)
    // We convert both 0 and -0 to zero by ignoring the first bit
    auto val_normalized = builder_.insert_and(
        builder_.insert_bitcast(value_type::u64(), val)->val(),
        builder_.insert_constant_u64(0x7FFFFFFFFFFFFFFF)->val());
    // If the value is 0 set 0b01 else set 0b00
    auto tag_val_default = builder_.insert_cmpeq(
        val_normalized->val(), builder_.insert_constant_u64(0b00)->val());
    tag_val_default =
        builder_.insert_zx(value_type::u16(), tag_val_default->val());
    auto c1_default = builder_.insert_constant_u16(0b0);

    // Overflow block
    // Set special tag
    auto tag_val_overflow = builder_.insert_constant_u16(0b10);
    // Set -nan with first bit of fraction set
    auto val_u64 = builder_.insert_constant_u64(0xFFF8000000000000);
    auto val_overflow =
        builder_.insert_bitcast(value_type::f64(), val_u64->val());
    auto c1_overflow = builder_.insert_constant_u16(0b1);

    // Write values
    auto tag_val =
        builder_.insert_csel(overflow_check_val->val(), tag_val_default->val(),
                             tag_val_overflow->val());
    fpu_tag_set(7, tag_val->val());
    auto stack_val = builder_.insert_csel(overflow_check_val->val(),
                                          orig_val->val(), val_overflow->val());
    fpu_stack_set(7, stack_val->val());

    auto c1_val = builder_.insert_csel(overflow_check_val->val(),
                                       c1_default->val(), c1_overflow->val());
    auto status = read_reg(value_type::u16(), reg_offsets::X87_STS);
    status = builder_.insert_bit_insert(status->val(), c1_val->val(), 9, 1);
    write_reg(reg_offsets::X87_STS, status->val());

    // We have to set this last, otherwise e.g. `FLD st(2)` would point to the
    // wrong register when copying
    return fpu_stack_index_move(-1);
}

action_node *translator::fpu_pop() {
    // TODO: FPU: Check fpu flag behavior (for c1) in case of underflow

    fpu_tag_set(0, builder_.insert_constant_u16(0b11)->val());
    // A unused X87 isn't actually cleared register
    // fpu_stack_set(0, builder_.insert_constant_u64(0x0)->val());
    return fpu_stack_index_move(1);
}

action_node *translator::write_reg(reg_offsets reg, port &value) {
    return builder_.insert_write_reg((unsigned long)reg, offset_to_idx(reg),
                                     offset_to_name(reg), value);
}

value_node *translator::read_reg(const value_type &vt, reg_offsets reg) {
    return builder_.insert_read_reg(vt, (unsigned long)reg, offset_to_idx(reg),
                                    offset_to_name(reg));
}

void translator::write_flags(value_node *op, flag_op zf, flag_op cf, flag_op of,
                             flag_op sf, flag_op pf, flag_op af) {
    switch (zf) {
    case flag_op::set0:
        write_reg(reg_offsets::ZF,
                  builder_.insert_constant_i(value_type::u1(), 0)->val());
        break;

    case flag_op::set1:
        write_reg(reg_offsets::ZF,
                  builder_.insert_constant_i(value_type::u1(), 1)->val());
        break;

    case flag_op::update:
        switch (op->kind()) {
        case node_kinds::unary_arith:
        case node_kinds::binary_arith:
        case node_kinds::ternary_arith:
            write_reg(reg_offsets::ZF, ((arith_node *)op)->zero());
            break;
        case node_kinds::unary_atomic:
        case node_kinds::binary_atomic:
        case node_kinds::ternary_atomic:
            write_reg(reg_offsets::ZF, ((atomic_node *)op)->zero());
            break;
        case node_kinds::bit_shift:
            write_reg(reg_offsets::ZF, ((bit_shift_node *)op)->zero());
            break;
        case node_kinds::constant:
            write_reg(reg_offsets::ZF,
                      builder_
                          .insert_constant_i(value_type::u1(),
                                             ((constant_node *)op)->is_zero())
                          ->val());
            break;
        default:
            throw std::runtime_error("unsupported operation node type for ZF");
        }
        break;

    default:
        break;
    }

    switch (cf) {
    case flag_op::set0:
        write_reg(reg_offsets::CF,
                  builder_.insert_constant_i(value_type::u1(), 0)->val());
        break;

    case flag_op::set1:
        write_reg(reg_offsets::CF,
                  builder_.insert_constant_i(value_type::u1(), 1)->val());
        break;

    case flag_op::update:
        switch (op->kind()) {
        case node_kinds::binary_arith:
        case node_kinds::ternary_arith:
            write_reg(reg_offsets::CF, ((arith_node *)op)->carry());
            break;
        case node_kinds::binary_atomic:
        case node_kinds::ternary_atomic:
            write_reg(reg_offsets::CF, ((atomic_node *)op)->carry());
            break;
        default:
            throw std::runtime_error("unsupported operation node type for CF");
        }
        break;

    default:
        break;
    }

    switch (of) {
    case flag_op::set0:
        write_reg(reg_offsets::OF,
                  builder_.insert_constant_i(value_type::u1(), 0)->val());
        break;

    case flag_op::set1:
        write_reg(reg_offsets::OF,
                  builder_.insert_constant_i(value_type::u1(), 1)->val());
        break;

    case flag_op::update:
        switch (op->kind()) {
        case node_kinds::binary_arith:
        case node_kinds::ternary_arith:
            write_reg(reg_offsets::OF, ((arith_node *)op)->overflow());
            break;
        case node_kinds::binary_atomic:
        case node_kinds::ternary_atomic:
            write_reg(reg_offsets::OF, ((atomic_node *)op)->overflow());
            break;
        default:
            throw std::runtime_error("unsupported operation node type for OF");
        }
        break;

    default:
        break;
    }

    switch (sf) {
    case flag_op::set0:
        write_reg(reg_offsets::SF,
                  builder_.insert_constant_i(value_type::u1(), 0)->val());
        break;

    case flag_op::set1:
        write_reg(reg_offsets::SF,
                  builder_.insert_constant_i(value_type::u1(), 1)->val());
        break;

    case flag_op::update:
        switch (op->kind()) {
        case node_kinds::unary_arith:
        case node_kinds::binary_arith:
        case node_kinds::ternary_arith:
            write_reg(reg_offsets::SF, ((arith_node *)op)->negative());
            break;
        case node_kinds::unary_atomic:
        case node_kinds::binary_atomic:
        case node_kinds::ternary_atomic:
            write_reg(reg_offsets::SF, ((atomic_node *)op)->negative());
            break;
        case node_kinds::bit_shift:
            write_reg(reg_offsets::SF, ((bit_shift_node *)op)->negative());
            break;
        case node_kinds::constant:
            write_reg(
                reg_offsets::SF,
                builder_
                    .insert_constant_i(value_type::u1(),
                                       ((constant_node *)op)->const_val_i() < 0)
                    ->val());
            break;
        default:
            throw std::runtime_error("unsupported operation node type for SF");
        }
        break;

    default:
        break;
    }
}

value_node *translator::compute_cond(cond_type ct) {
    switch (ct) {
    case cond_type::nbe: {
        auto ncf = builder_.insert_not(
            read_reg(value_type::u1(), reg_offsets::CF)->val());
        auto nzf = builder_.insert_not(
            read_reg(value_type::u1(), reg_offsets::ZF)->val());
        return builder_.insert_and(ncf->val(), nzf->val());
    }

    case cond_type::nb: {
        auto ncf = builder_.insert_not(
            read_reg(value_type::u1(), reg_offsets::CF)->val());
        return ncf;
    }

    case cond_type::b: {
        auto cf = read_reg(value_type::u1(), reg_offsets::CF);
        return cf;
    }

    case cond_type::be: {
        auto cf = read_reg(value_type::u1(), reg_offsets::CF);
        auto zf = read_reg(value_type::u1(), reg_offsets::ZF);
        return builder_.insert_or(cf->val(), zf->val());
    }

    case cond_type::z: {
        auto zf = read_reg(value_type::u1(), reg_offsets::ZF);
        return zf;
    }

    case cond_type::nle: {
        auto nzf = builder_.insert_not(
            read_reg(value_type::u1(), reg_offsets::ZF)->val());
        auto sf = read_reg(value_type::u1(), reg_offsets::SF);
        auto of = read_reg(value_type::u1(), reg_offsets::OF);

        return builder_.insert_and(
            nzf->val(), builder_.insert_cmpeq(sf->val(), of->val())->val());
    }

    case cond_type::nl: {
        auto sf = read_reg(value_type::u1(), reg_offsets::SF);
        auto of = read_reg(value_type::u1(), reg_offsets::OF);

        return builder_.insert_cmpeq(sf->val(), of->val());
    }

    case cond_type::l: {
        auto sf = read_reg(value_type::u1(), reg_offsets::SF);
        auto of = read_reg(value_type::u1(), reg_offsets::OF);

        return builder_.insert_cmpne(sf->val(), of->val());
    }

    case cond_type::le: {
        auto zf = read_reg(value_type::u1(), reg_offsets::ZF);
        auto sf = read_reg(value_type::u1(), reg_offsets::SF);
        auto of = read_reg(value_type::u1(), reg_offsets::OF);

        return builder_.insert_or(
            zf->val(), builder_.insert_cmpne(sf->val(), of->val())->val());
    }

    case cond_type::nz: {
        auto nzf = builder_.insert_not(
            read_reg(value_type::u1(), reg_offsets::ZF)->val());
        return nzf;
    }

    case cond_type::no: {
        auto nof = builder_.insert_not(
            read_reg(value_type::u1(), reg_offsets::OF)->val());
        return nof;
    }

    case cond_type::np: {
        auto npf = builder_.insert_not(
            read_reg(value_type::u1(), reg_offsets::PF)->val());
        return npf;
    }

    case cond_type::ns: {
        auto nsf = builder_.insert_not(
            read_reg(value_type::u1(), reg_offsets::SF)->val());
        return nsf;
    }

    case cond_type::o: {
        auto of = read_reg(value_type::u1(), reg_offsets::OF);
        return of;
    }

    case cond_type::p: {
        auto pf = read_reg(value_type::u1(), reg_offsets::PF);
        return pf;
    }

    case cond_type::s: {
        auto sf = read_reg(value_type::u1(), reg_offsets::SF);
        return sf;
    }

    default:
        throw std::runtime_error("unhandled condition code");
    }
}

value_node *translator::auto_cast(const value_type &target_type,
                                  value_node *v) {
    const auto &vtype = v->val().type();

    // If the widths of the type are the same, then we might only have to do a
    // bitcast.
    if (target_type.width() == vtype.width()) {
        if (target_type.type_class() != vtype.type_class()) {
            // If the type classes are different, then just do a bitcast.
            return builder_.insert_bitcast(target_type, v->val());
        } else {
            // Otherwise, there's actually nothing to do.
            return v;
        }
    }

    // If we get here, we know we need to do a truncation, or an extension.
    value_node *r;

    if (target_type.width() < vtype.width()) {
        // This is a truncation
        r = builder_.insert_trunc(
            value_type(vtype.type_class(), target_type.width()), v->val());
    } else {
        // This is an extension
        if (vtype.type_class() == value_type_class::signed_integer) {
            // The value is a signed integer, so do a sign extension.
            r = builder_.insert_sx(value_type(value_type_class::signed_integer,
                                              target_type.width()),
                                   v->val());
        } else if (vtype.type_class() == value_type_class::unsigned_integer) {
            // The value is an unsigned integer, so do a zero extension.
            r = builder_.insert_zx(
                value_type(value_type_class::unsigned_integer,
                           target_type.width()),
                v->val());
        } else {
            // Other type classes (void, floating point, etc) are not supported.
            throw std::runtime_error("auto cast not supported");
        }
    }

    // We've done an extension or a truncation, but we might also have to do a
    // bitcast, e.g. if we're sign extensing a s32 to u64, we have to sign
    // extend, but then bitcast.
    if (target_type.type_class() != vtype.type_class()) {
        r = builder_.insert_bitcast(target_type, r->val());
    }

    return r;
}

value_type translator::type_of_operand(int opnum) {
    const xed_inst_t *insn = xed_decoded_inst_inst(xed_inst());
    auto operand = xed_inst_operand(insn, opnum);
    auto opname = xed_operand_name(operand);

    switch (opname) {
    case XED_OPERAND_REG0:
    case XED_OPERAND_REG1: {
        auto reg = xed_decoded_inst_get_reg(xed_inst(), opname);
        auto regclass = xed_reg_class(reg);

        switch (regclass) {
        case XED_REG_CLASS_GPR:
            return value_type(value_type_class::unsigned_integer,
                              xed_get_register_width_bits(reg));

        case XED_REG_CLASS_XMM:
            return value_type(value_type_class::unsigned_integer, 128);

        default:
            throw std::runtime_error("unsupported register class");
        }
    }

    case XED_OPERAND_IMM0: {
        return value_type(
            value_type_class::signed_integer,
            xed_decoded_inst_get_immediate_width_bits(xed_inst()));
    }

    default:
        throw std::logic_error("unsupported operand type: " +
                               std::to_string((int)opname));
    }
}
