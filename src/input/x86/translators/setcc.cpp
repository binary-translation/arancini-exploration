#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void setcc_translator::do_translate()
{
	value_node *cond;
	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_SETNBE:
		cond = compute_cond(cond_type::nbe);
		break;
	case XED_ICLASS_SETNB:
		cond = compute_cond(cond_type::nb);
		break;
	case XED_ICLASS_SETB:
		cond = compute_cond(cond_type::b);
		break;
	case XED_ICLASS_SETBE:
		cond = compute_cond(cond_type::be);
		break;
	case XED_ICLASS_SETZ:
		cond = compute_cond(cond_type::z);
		break;
	case XED_ICLASS_SETNLE:
		cond = compute_cond(cond_type::nle);
		break;
	case XED_ICLASS_SETNL:
		cond = compute_cond(cond_type::nl);
		break;
	case XED_ICLASS_SETL:
		cond = compute_cond(cond_type::l);
		break;
	case XED_ICLASS_SETLE:
		cond = compute_cond(cond_type::le);
		break;
	case XED_ICLASS_SETNZ:
		cond = compute_cond(cond_type::nz);
		break;
	case XED_ICLASS_SETNO:
		cond = compute_cond(cond_type::no);
		break;
	case XED_ICLASS_SETNP:
		cond = compute_cond(cond_type::np);
		break;
	case XED_ICLASS_SETNS:
		cond = compute_cond(cond_type::ns);
		break;
	case XED_ICLASS_SETO:
		cond = compute_cond(cond_type::o);
		break;
	case XED_ICLASS_SETP:
		cond = compute_cond(cond_type::p);
		break;
	case XED_ICLASS_SETS:
		cond = compute_cond(cond_type::s);
		break;

	default:
		throw std::runtime_error("unhandled setcc instruction");
	}

	write_operand(0, cond->val());
}
