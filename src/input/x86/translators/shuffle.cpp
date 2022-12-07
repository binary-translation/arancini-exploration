#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/ir-builder.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void shuffle_translator::do_translate()
{
	switch (xed_decoded_inst_get_iclass(xed_inst())) {
    case XED_ICLASS_PSHUFD: {
        auto dst = read_operand(0);
        auto src = read_operand(1);
        auto order = read_operand(2);

        for (int i = 0; i < 4; i++) {
            auto shift = builder().insert_asr(src->val(), builder().insert_bit_extract(order->val(), 2 * i, 2)->val());
            auto res = builder().insert_trunc(value_type::u32(), shift->val());
            dst = builder().insert_bit_insert(dst->val(), res->val(), 32 * i, 32);
        }

        write_operand(0, dst->val());
        break;
    }

	default:
		throw std::runtime_error("unsupported shuffle instruction");
	}
}