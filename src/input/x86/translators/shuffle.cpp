#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void shuffle_translator::do_translate() {
    switch (xed_decoded_inst_get_iclass(xed_inst())) {
    case XED_ICLASS_PSHUFHW: {
        auto dst = builder().insert_bitcast(
            value_type::vector(value_type::u16(), 8), read_operand(0)->val());
        auto src = builder().insert_bitcast(
            value_type::vector(value_type::u16(), 8), read_operand(1)->val());
        auto slct = ((constant_node *)read_operand(2))->const_val_i();

        // shuffle high quadword
        for (auto i = 0; i < 4; i++) {
            auto tmp = builder().insert_vector_extract(
                src->val(), ((slct >> (i * 2)) & 3) + 4);
            dst = builder().insert_vector_insert(dst->val(), i + 4, tmp->val());
        }
        // keep low quadword
        for (auto i = 0; i < 4; i++) {
            auto tmp = builder().insert_vector_extract(src->val(), i);
            dst = builder().insert_vector_insert(dst->val(), i, tmp->val());
        }

        write_operand(0, dst->val());
        break;
    }
    case XED_ICLASS_PSHUFLW: {
        auto dst = builder().insert_bitcast(
            value_type::vector(value_type::u16(), 8), read_operand(0)->val());
        auto src = builder().insert_bitcast(
            value_type::vector(value_type::u16(), 8), read_operand(1)->val());
        auto slct = ((constant_node *)read_operand(2))->const_val_i();

        // shuffle lower quadword
        for (auto i = 0; i < 4; i++) {
            auto tmp = builder().insert_vector_extract(src->val(),
                                                       (slct >> (i * 2)) & 3);
            dst = builder().insert_vector_insert(dst->val(), i, tmp->val());
        }
        // keep upper quadword
        for (auto i = 4; i < 8; i++) {
            auto tmp = builder().insert_vector_extract(src->val(), i);
            dst = builder().insert_vector_insert(dst->val(), i, tmp->val());
        }

        write_operand(0, dst->val());
        break;
    }
    case XED_ICLASS_PSHUFD: {
        auto slct = ((constant_node *)read_operand(2))->const_val_i();
        auto dst_vec = builder().insert_bitcast(
            value_type::vector(value_type::u32(), 4), read_operand(0)->val());
        auto src_vec = builder().insert_bitcast(
            value_type::vector(value_type::u32(), 4), read_operand(1)->val());

        for (int i = 0; i < 4; i++) {
            auto res = builder().insert_vector_extract(
                src_vec->val(), (int)(slct >> (2 * i) & 0b11));
            dst_vec =
                builder().insert_vector_insert(dst_vec->val(), i, res->val());
        }

        write_operand(0, dst_vec->val());
        break;
    }

    case XED_ICLASS_SHUFPS:
    case XED_ICLASS_SHUFPD: {
        auto dst = read_operand(0);
        value_type vec_ty = value_type::v();
        unsigned long mask;
        unsigned bits;
        if (xed_decoded_inst_get_iclass(xed_inst()) == XED_ICLASS_SHUFPS) {
            vec_ty = value_type::vector(value_type::f32(), 4);
            mask = 3;
            bits = 2;
        } else {
            vec_ty = value_type::vector(value_type::f64(), 2);
            mask = 1;
            bits = 1;
        }
        auto src1 = auto_cast(vec_ty, read_operand(0));
        auto src2 = auto_cast(vec_ty, read_operand(1));
        auto slct = ((constant_node *)read_operand(2))->const_val_i();

        for (unsigned i = 0; i < vec_ty.nr_elements(); i++) {
            auto idx = (slct >> (bits * i)) & mask;
            auto from = (idx < vec_ty.nr_elements() / 2) ? src1 : src2;
            auto val = builder().insert_vector_extract(from->val(), idx);
            dst = builder().insert_vector_insert(dst->val(), i, val->val());
        }

        write_operand(0, dst->val());
        break;
    }

    default:
        throw std::runtime_error("unsupported shuffle instruction");
    }
}
