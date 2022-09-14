#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>
#include <arancini/ir/packet.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void rep_translator::do_translate()
{
	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_REPE_CMPSB: {
		// while ecx != 0 && zf != 0; do cmpsb; ecx--

		// cmpsb - compares byte at address DS:(E)SI with byte at address ES:(E)DI and sets the status flags accordingly

		auto loop_start = pkt()->insert_label();

		auto deref_rsi = pkt()->insert_read_mem(value_type::u8(), read_reg(value_type::u64(), reg_to_offset(XED_REG_RSI))->val());
		auto deref_rdi = pkt()->insert_read_mem(value_type::u8(), read_reg(value_type::u64(), reg_to_offset(XED_REG_RDI))->val());

		auto rslt = pkt()->insert_sub(deref_rsi->val(), deref_rdi->val());

		write_flags(rslt, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update, flag_op::update);

		// increment RSI+RDI

		// decrement ecx
		auto new_ecx = pkt()->insert_sub(read_reg(value_type::u32(), reg_to_offset(XED_REG_RCX))->val(), pkt()->insert_constant_u32(8)->val());
		write_reg(reg_to_offset(XED_REG_RCX), pkt()->insert_zx(value_type::u64(), new_ecx->val())->val());

		auto ecx_eq_0 = pkt()->insert_cmpeq(new_ecx->val(), pkt()->insert_constant_u32(0)->val());

		auto zf = read_reg(value_type::u1(), reg_offsets::zf);

		auto termcond = pkt()->insert_or(pkt()->insert_trunc(value_type::u1(), ecx_eq_0->val())->val(), pkt()->insert_not(zf->val())->val());

		pkt()->insert_cond_br(pkt()->insert_not(termcond->val())->val(), loop_start);

		break;
	}

	default:
		throw std::runtime_error("unsupported rep instruction");
	}
}
