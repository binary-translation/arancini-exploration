#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void punpck_translator::do_translate() {
    auto op0 = read_operand(0);
    auto op1 = read_operand(1);

    auto inst = xed_decoded_inst_get_iclass(xed_inst());
    switch (inst) {

    case XED_ICLASS_UNPCKHPD: {
        auto v0 = builder().insert_bitcast(
            value_type::vector(value_type::f64(), 2), op0->val());
        auto v1 = builder().insert_bitcast(
            value_type::vector(value_type::f64(), 2), op1->val());
        v0 = builder().insert_vector_extract(v0->val(), 1);
        v1 = builder().insert_vector_extract(v1->val(), 1);
        auto dst = builder().insert_bitcast(
            value_type::vector(value_type::f64(), 2), op0->val());
        dst = builder().insert_vector_insert(dst->val(), 0, v0->val());
        dst = builder().insert_vector_insert(dst->val(), 1, v1->val());
        write_operand(0, dst->val());
    }

    case XED_ICLASS_PUNPCKLBW:
    case XED_ICLASS_PUNPCKLWD:
    case XED_ICLASS_PUNPCKLDQ:
    case XED_ICLASS_PUNPCKLQDQ: {
        auto input_size = op0->val().type().width();
        auto elt_size = (inst == XED_ICLASS_PUNPCKLBW)   ? 8
                        : (inst == XED_ICLASS_PUNPCKLWD) ? 16
                        : (inst == XED_ICLASS_PUNPCKLDQ) ? 32
                                                         : 64;
        auto elt_type =
            value_type(value_type_class::unsigned_integer, elt_size);
        auto v0 = builder().insert_bitcast(
            value_type::vector(elt_type, input_size / elt_size), op0->val());
        auto v1 = builder().insert_bitcast(
            value_type::vector(elt_type, input_size / elt_size), op1->val());
        auto dst = v0;

        for (int i = 0; i < input_size / elt_size / 2; i++) {
            dst = builder().insert_vector_insert(
                dst->val(), i * 2,
                builder().insert_vector_extract(v0->val(), i)->val());
            dst = builder().insert_vector_insert(
                dst->val(), i * 2 + 1,
                builder().insert_vector_extract(v1->val(), i)->val());
        }

        write_operand(0, dst->val());
        break;
    }

    case XED_ICLASS_PUNPCKHBW:
    case XED_ICLASS_PUNPCKHWD:
    case XED_ICLASS_PUNPCKHDQ:
    case XED_ICLASS_PUNPCKHQDQ: {
        auto input_size = op0->val().type().width();
        auto elt_size = (inst == XED_ICLASS_PUNPCKHBW)   ? 8
                        : (inst == XED_ICLASS_PUNPCKHWD) ? 16
                        : (inst == XED_ICLASS_PUNPCKHDQ) ? 32
                                                         : 64;
        auto elt_type =
            value_type(value_type_class::unsigned_integer, elt_size);
        auto v0 = builder().insert_bitcast(
            value_type::vector(elt_type, input_size / elt_size), op0->val());
        auto v1 = builder().insert_bitcast(
            value_type::vector(elt_type, input_size / elt_size), op1->val());
        auto dst = v0;

        for (int i = 0; i < input_size / elt_size / 2; i++) {
            dst = builder().insert_vector_insert(
                dst->val(), 2 * i,
                builder()
                    .insert_vector_extract(
                        v0->val(), i + v0->val().type().nr_elements() / 2)
                    ->val());
            dst = builder().insert_vector_insert(
                dst->val(), 2 * i + 1,
                builder()
                    .insert_vector_extract(
                        v1->val(), i + v1->val().type().nr_elements() / 2)
                    ->val());
        }

        write_operand(0, dst->val());
        break;
    }

    case XED_ICLASS_PACKUSWB: {
        /*
         * DEST[7:0] := SaturateSignedWordToUnsignedByte DEST[15:0];
         * DEST[15:8] := SaturateSignedWordToUnsignedByte DEST[31:16];
         * DEST[23:16] := SaturateSignedWordToUnsignedByte DEST[47:32];
         * DEST[31:24] := SaturateSignedWordToUnsignedByte DEST[63:48];
         * DEST[39:32] := SaturateSignedWordToUnsignedByte SRC[15:0];
         * DEST[47:40] := SaturateSignedWordToUnsignedByte SRC[31:16];
         * DEST[55:48] := SaturateSignedWordToUnsignedByte SRC[47:32];
         * DEST[63:56] := SaturateSignedWordToUnsignedByte SRC[63:48];
         * (until 128 bits for xmm inputs)
         * Saturate: if signed word > unsigned byte, make it 0xFF, if negative,
         * make it 0x00
         */
        auto nr_splits = (op0->val().type().width() == 64) ? 4 : 8;
        auto dst = builder().insert_bitcast(
            value_type::vector(value_type::s16(), nr_splits), op0->val());
        auto src = builder().insert_bitcast(
            value_type::vector(value_type::s16(), nr_splits), op1->val());
        auto dst_bytes = builder().insert_bitcast(
            value_type::vector(value_type::s8(), nr_splits * 2), op0->val());

        for (int i = 0; i < nr_splits * 2; i++) {
            auto word = (i < nr_splits)
                            ? builder().insert_vector_extract(dst->val(), i)
                            : builder().insert_vector_extract(src->val(),
                                                              i - nr_splits);
            auto neg_test = builder().insert_cmpgt(
                builder().insert_constant_s16(0)->val(), word->val());
            cond_br_node *br_neg = (cond_br_node *)builder().insert_cond_br(
                neg_test->val(), nullptr);
            auto overflow_test = builder().insert_cmpne(
                builder()
                    .insert_and(word->val(),
                                builder().insert_constant_s16(0xFF00)->val())
                    ->val(),
                builder().insert_constant_s16(0)->val());
            cond_br_node *br_of = (cond_br_node *)builder().insert_cond_br(
                overflow_test->val(), nullptr);

            // word fits in u8, set dst to truncated word
            auto unsign =
                builder().insert_bitcast(value_type::u16(), word->val());
            dst_bytes = builder().insert_vector_insert(
                dst_bytes->val(), i,
                builder().insert_trunc(value_type::u8(), unsign->val())->val());
            br_node *br_end = (br_node *)builder().insert_br(nullptr);

            // word is negative, set dst to 0x00
            auto neg_label = builder().insert_label("negative");
            br_neg->add_br_target(neg_label);
            dst_bytes = builder().insert_vector_insert(
                dst_bytes->val(), i, builder().insert_constant_u8(0)->val());
            br_node *br_end2 = (br_node *)builder().insert_br(nullptr);

            // word overflows for 8-bit unsigned, set dst to 0xFF
            auto of_label = builder().insert_label("overflow");
            br_of->add_br_target(of_label);
            dst_bytes = builder().insert_vector_insert(
                dst_bytes->val(), i, builder().insert_constant_u8(0xFF)->val());

            auto end_label = builder().insert_label("end");
            auto end_label1 = builder().insert_label("end");
            br_end->add_br_target(end_label);
            br_end2->add_br_target(end_label1);
        }
        write_operand(0, dst_bytes->val());
        break;
    }
    case XED_ICLASS_PACKSSDW:
    case XED_ICLASS_PACKSSWB: {
        auto bits = op0->val().type().width();
        auto pre_ty = value_type::v();
        auto pst_ty = value_type::v();
        value_node *cmp_max;
        value_node *cmp_min;
        value_node *ins_max;
        value_node *ins_min;
        switch (inst) {
        case XED_ICLASS_PACKSSDW: {
            pre_ty = value_type::s32();
            pst_ty = value_type::s16();
            cmp_max = builder().insert_constant_s32(0x00007FFF);
            cmp_min = builder().insert_constant_s32(0xFFFF8000);
            ins_max = builder().insert_constant_s16(0x7FFF);
            ins_min = builder().insert_constant_s16(0x8000);
        } break;
        case XED_ICLASS_PACKSSWB: {
            pre_ty = value_type::s16();
            pst_ty = value_type::s8();
            cmp_max = builder().insert_constant_s16(0x007F);
            cmp_min = builder().insert_constant_s16(0xFF80);
            ins_max = builder().insert_constant_s8(0x7F);
            ins_min = builder().insert_constant_s8(0x80);
        } break;
        default:
            throw std::logic_error(
                "XED instruction class not handled in switch");
        }
        auto dst = builder().insert_bitcast(
            value_type::vector(pre_ty, bits / pre_ty.width()), op0->val());
        auto src = builder().insert_bitcast(
            value_type::vector(pre_ty, bits / pre_ty.width()), op1->val());
        auto result = builder().insert_bitcast(
            value_type::vector(pst_ty, bits / pst_ty.width()), op0->val());

        for (std::size_t i = 0; i < bits / pst_ty.width(); i++) {

            auto tmp = (i < bits / pre_ty.width())
                           ? builder().insert_vector_extract(dst->val(), i)
                           : builder().insert_vector_extract(
                                 src->val(), i - bits / pre_ty.width());
            auto gt_max = builder().insert_cmpgt(tmp->val(), cmp_max->val());
            auto lt_min = builder().insert_cmpgt(cmp_min->val(), tmp->val());

            tmp = builder().insert_trunc(pst_ty, tmp->val());

            tmp = builder().insert_csel(lt_min->val(), ins_min->val(),
                                        tmp->val());
            tmp = builder().insert_csel(gt_max->val(), ins_max->val(),
                                        tmp->val());
            result =
                builder().insert_vector_insert(result->val(), i, tmp->val());
        }
        write_operand(0, result->val());
        break;
    }
    case XED_ICLASS_PEXTRW: {

        auto index = read_operand(2);
        auto idx = ((constant_node *)index)->const_val_i() & 7;

        auto src = builder().insert_bitcast(
            value_type::vector(value_type::u16(), 8), op1->val());
        auto res = builder().insert_vector_extract(src->val(), idx);

        write_operand(0, res->val());
        break;
    }

    default:
        throw std::runtime_error("unsupported punpck operation");
    }
}
