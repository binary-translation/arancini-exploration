#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void atomic_translator::do_translate()
{
	switch (xed_decoded_inst_get_iclass(xed_inst())) {
    case XED_ICLASS_XADD_LOCK: {
        // if LOCK prefix, dst is a memory address
        auto dst = compute_address(0);
        auto src = read_operand(1);

        auto xadd = builder().insert_atomic_xadd(dst->val(), src->val());
        write_flags(xadd, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update);

        break;
    }

    default:
        throw std::runtime_error("unsupported atomic instruction");
    }
}