#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/ir-builder.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void rep_translator::do_translate()
{
	auto inst_class = xed_decoded_inst_get_iclass(xed_inst());

  switch (inst_class) {
	case XED_ICLASS_REPE_CMPSB: {
		// while ecx != 0 && zf != 0; do cmpsb; ecx--

		// cmpsb - compares byte at address DS:(E)SI with byte at address ES:(E)DI and sets the status flags accordingly

		auto loop_start = builder().insert_label();

		auto deref_rsi = builder().insert_read_mem(value_type::u8(), read_reg(value_type::u64(), reg_offsets::RSI)->val());
		auto deref_rdi = builder().insert_read_mem(value_type::u8(), read_reg(value_type::u64(), reg_offsets::RDI)->val());

		auto rslt = builder().insert_sub(deref_rsi->val(), deref_rdi->val());

		write_flags(rslt, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update);

		// increment RSI+RDI

		// decrement ecx
		auto new_ecx = builder().insert_sub(read_reg(value_type::u32(), reg_offsets::RCX)->val(), builder().insert_constant_u32(8)->val());
		write_reg(reg_offsets::RCX, builder().insert_zx(value_type::u64(), new_ecx->val())->val());

		auto ecx_eq_0 = builder().insert_cmpeq(new_ecx->val(), builder().insert_constant_u32(0)->val());

		auto zf = read_reg(value_type::u1(), reg_offsets::ZF);

		auto termcond = builder().insert_or(builder().insert_trunc(value_type::u1(), ecx_eq_0->val())->val(), builder().insert_not(zf->val())->val());

		builder().insert_cond_br(builder().insert_not(termcond->val())->val(), loop_start);

		break;
	}

  case XED_ICLASS_REP_STOSB:
  case XED_ICLASS_REP_STOSD:
  case XED_ICLASS_REP_STOSW:
	case XED_ICLASS_REP_STOSQ: {
		// while rcx != 0; do stosq; rcx--; rdi++; done
		// stosq - store the content of rax at [rdi]
		// xed operand encoding: stosq mem base0 rax

		auto cst_0 = builder().insert_constant_u64(0);
		auto cst_1 = builder().insert_constant_u64(1);
    int addr_align;

    switch (inst_class) {
    case XED_ICLASS_REP_STOSQ:
      addr_align = 8;
      break;
    case XED_ICLASS_REP_STOSD:
      addr_align = 4;
      break;
    case XED_ICLASS_REP_STOSW:
      addr_align = 2;
      break;
    case XED_ICLASS_REP_STOSB:
      addr_align = 1;
      break;
    default:
      throw std::runtime_error("unsupported rep stos size");
    }
		auto cst_align = builder().insert_constant_u64(addr_align);

		auto loop_start = builder().insert_label("while");
		auto rcx = read_reg(value_type::u64(), reg_offsets::RCX);
		auto rcx_test = builder().insert_cmpeq(rcx->val(), cst_0->val());
		cond_br_node *br_loop = (cond_br_node *)builder().insert_cond_br(rcx_test->val(), nullptr);

		auto rax = read_operand(2);
		auto rdi = read_reg(value_type::u64(), reg_offsets::RDI);
		builder().insert_write_mem(rdi->val(), rax->val());

		auto df = read_reg(value_type::u1(), reg_offsets::DF);
		auto df_test = builder().insert_cmpne(df->val(), builder().insert_constant_i(value_type::u1(), 0)->val());
		cond_br_node *br_df = (cond_br_node *)builder().insert_cond_br(df_test->val(), nullptr);

		write_reg(reg_offsets::RDI, builder().insert_add(rdi->val(), cst_align->val())->val());
		br_node *br_then = (br_node *)builder().insert_br(nullptr);

		auto else_label = builder().insert_label("else");
		br_df->add_br_target(else_label);

		write_reg(reg_offsets::RDI, builder().insert_sub(rdi->val(), cst_align->val())->val());

		auto endif_label = builder().insert_label("endif");
		br_then->add_br_target(endif_label);

		write_reg(reg_offsets::RCX, builder().insert_sub(rcx->val(), cst_1->val())->val());
		builder().insert_br(loop_start);

		auto loop_end = builder().insert_label("end");
		br_loop->add_br_target(loop_end);

		break;
	}

  case XED_ICLASS_REP_MOVSB:
  case XED_ICLASS_REP_MOVSD:
  case XED_ICLASS_REP_MOVSW:
  case XED_ICLASS_REP_MOVSQ: {
    // while rcx != 0; do movsq [rdi], [rsi]; rcx--; rsi++; rdi++; done
    // movsq [rdi], [rsi] - move the value in memory at rsi to the address at rdi

		auto cst_0 = builder().insert_constant_u64(0);
		auto cst_1 = builder().insert_constant_u64(1);
    int addr_align;

		switch (inst_class) {
    case XED_ICLASS_REP_MOVSQ:
      addr_align = 8;
      break;
    case XED_ICLASS_REP_MOVSD:
      addr_align = 4;
      break;
    case XED_ICLASS_REP_MOVSW:
      addr_align = 2;
      break;
    case XED_ICLASS_REP_MOVSB:
      addr_align = 1;
      break;
    default:
      throw std::runtime_error("unsupported rep movs size");
    }
		auto cst_align = builder().insert_constant_u64(addr_align);

		// while rcx != 0
		auto loop_start = builder().insert_label("while");
		auto rcx = read_reg(value_type::u64(), reg_offsets::RCX);
		auto rcx_test = builder().insert_cmpeq(rcx->val(), cst_0->val());
		cond_br_node *br_loop = (cond_br_node *)builder().insert_cond_br(rcx_test->val(), nullptr);

    // movsq [rdi], [rsi]
		auto rsi = read_reg(value_type::u64(), reg_offsets::RSI);
	    const value_type &vt = addr_align == 8 ? value_type::u64() : addr_align == 4 ? value_type::u32() : addr_align == 2 ? value_type::u16() : value_type::u8();
	  	auto rsi_val = builder().insert_read_mem(vt, rsi->val());
		auto rdi = read_reg(value_type::u64(), reg_offsets::RDI);
		builder().insert_write_mem(rdi->val(), rsi_val->val());

    // update rdi and rsi according to DF register (0: inc, 1: dec)
		auto df = read_reg(value_type::u1(), reg_offsets::DF);
		auto df_test = builder().insert_cmpne(df->val(), builder().insert_constant_i(value_type::u1(), 0)->val());
		cond_br_node *br_df = (cond_br_node *)builder().insert_cond_br(df_test->val(), nullptr);

		write_reg(reg_offsets::RDI, builder().insert_add(rdi->val(), cst_align->val())->val());
		write_reg(reg_offsets::RSI, builder().insert_add(rsi->val(), cst_align->val())->val());
		br_node *br_then = (br_node *)builder().insert_br(nullptr);

		auto else_label = builder().insert_label("else");
		br_df->add_br_target(else_label);

		write_reg(reg_offsets::RDI, builder().insert_sub(rdi->val(), cst_align->val())->val());
		write_reg(reg_offsets::RSI, builder().insert_sub(rsi->val(), cst_align->val())->val());

		auto endif_label = builder().insert_label("endif");
		br_then->add_br_target(endif_label);

    // rcx--
		write_reg(reg_offsets::RCX, builder().insert_sub(rcx->val(), cst_1->val())->val());
		builder().insert_br(loop_start);

		auto loop_end = builder().insert_label("end");
		br_loop->add_br_target(loop_end);

    break;
  }

	default:
		throw std::runtime_error("unsupported rep instruction");
	}
}
