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

  case XED_ICLASS_XCHG: {
    // if an operand is memory, xchg becomes atomic
    if (is_memory_operand(0)) {
      builder().insert_atomic_xchg(compute_address(0)->val(), read_operand(1)->val());
    } else if (is_memory_operand(1)) {
      builder().insert_atomic_xchg(compute_address(1)->val(), read_operand(0)->val());
    } else {
      // TODO fix with a temp register node
      write_operand(0, read_operand(1)->val());
      write_operand(1, read_operand(0)->val());
    }
    break;
  }

  case XED_ICLASS_CMPXCHG_LOCK: {
    auto dst = compute_address(0);
    auto acc = read_operand(1);
    auto src = read_operand(2);

    auto res = builder().insert_atomic_cmpxchg(dst->val(), acc->val(), src->val());
    write_flags(res, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update);
    break;
  }

  case XED_ICLASS_ADD_LOCK: {
    auto dst = compute_address(0);
    auto src = read_operand(1);

    auto res = builder().insert_atomic_binop(binary_atomic_op::add, dst->val(), src->val());
    write_flags(res, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update);
    break;
  }

  case XED_ICLASS_AND_LOCK: {
    auto op0 = compute_address(0);
    auto op1 = read_operand(1);

    auto res = builder().insert_atomic_binop(binary_atomic_op::band, op0->val(), op1->val());
    write_flags(res, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::ignore);
    break;
  }

  case XED_ICLASS_OR_LOCK: {
    auto op0 = compute_address(0);
    auto op1 = read_operand(1);

    auto res = builder().insert_atomic_binop(binary_atomic_op::bor, op0->val(), op1->val());
    write_flags(res, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::ignore);
    break;
	}

  case XED_ICLASS_DEC_LOCK: {
    auto src = compute_address(0);
    builder().insert_atomic_binop(binary_atomic_op::sub, src->val(), builder().insert_constant_i(src->val().type(), 1)->val());
    break;
  }

  default:
    throw std::runtime_error("unsupported atomic instruction");
  }
}
