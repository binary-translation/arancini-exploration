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
    auto op0 = read_operand(0);
    auto op1 = read_operand(1);

    if (is_memory_operand(0) || is_memory_operand(1)) { // atomic
      builder().insert_atomic_xchg(op0->val(), op1->val());
    } else { // non-atomic
      write_operand(0, op1->val());
      write_operand(1, op0->val());
    }
    break;
  }

  case XED_ICLASS_CMPXCHG_LOCK: {
    auto dst = read_operand(0);
    auto acc = read_operand(1);
    auto src = read_operand(2);

    auto res = builder().insert_atomic_cmpxchg(dst->val(), acc->val(), src->val());
    write_flags(res, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update);
    break;
  }

  case XED_ICLASS_ADD_LOCK: {
    auto op0 = read_operand(0);
    auto op1 = read_operand(1);

    auto res = builder().insert_atomic_binop(binary_atomic_op::add, op0->val(), op1->val());
    write_flags(res, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update);
    break;
  }

  case XED_ICLASS_AND_LOCK: {
    auto op0 = read_operand(0);
    auto op1 = read_operand(1);

    auto res = builder().insert_atomic_binop(binary_atomic_op::band, op0->val(), op1->val());
    write_flags(res, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::ignore);
    break;
  }

  case XED_ICLASS_OR_LOCK: {
    auto op0 = read_operand(0);
    auto op1 = read_operand(1);

    auto res = builder().insert_atomic_binop(binary_atomic_op::bor, op0->val(), op1->val());
    write_flags(res, flag_op::update, flag_op::set0, flag_op::set0, flag_op::update, flag_op::update, flag_op::ignore);
    break;
	}

  case XED_ICLASS_INC_LOCK: {
    auto src = read_operand(0);
    auto res = builder().insert_atomic_binop(binary_atomic_op::add, src->val(), builder().insert_constant_i(src->val().type(), 1)->val());
    write_flags(res, flag_op::update, flag_op::ignore, flag_op::update, flag_op::update, flag_op::update, flag_op::update);
    break;
  }

  case XED_ICLASS_DEC_LOCK: {
    auto src = read_operand(0);
    auto res = builder().insert_atomic_binop(binary_atomic_op::sub, src->val(), builder().insert_constant_i(src->val().type(), 1)->val());
    write_flags(res, flag_op::update, flag_op::ignore, flag_op::update, flag_op::update, flag_op::update, flag_op::update);
    break;
  }

  default:
    throw std::runtime_error("unsupported atomic instruction");
  }
}
