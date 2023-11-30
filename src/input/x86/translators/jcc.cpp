#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void jcc_translator::do_translate()
{
	value_node *cond;
	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_JNBE:
		cond = compute_cond(cond_type::nbe);
		break;
	case XED_ICLASS_JNB:
		cond = compute_cond(cond_type::nb);
		break;
	case XED_ICLASS_JB:
		cond = compute_cond(cond_type::b);
		break;
	case XED_ICLASS_JBE:
		cond = compute_cond(cond_type::be);
		break;
	case XED_ICLASS_JZ:
		cond = compute_cond(cond_type::z);
		break;
	case XED_ICLASS_JNLE:
		cond = compute_cond(cond_type::nle);
		break;
	case XED_ICLASS_JNL:
		cond = compute_cond(cond_type::nl);
		break;
	case XED_ICLASS_JL:
		cond = compute_cond(cond_type::l);
		break;
	case XED_ICLASS_JLE:
		cond = compute_cond(cond_type::le);
		break;
	case XED_ICLASS_JNZ:
		cond = compute_cond(cond_type::nz);
		break;
	case XED_ICLASS_JNO:
		cond = compute_cond(cond_type::no);
		break;
	case XED_ICLASS_JNP:
		cond = compute_cond(cond_type::np);
		break;
	case XED_ICLASS_JNS:
		cond = compute_cond(cond_type::ns);
		break;
	case XED_ICLASS_JO:
		cond = compute_cond(cond_type::o);
		break;
	case XED_ICLASS_JP:
		cond = compute_cond(cond_type::p);
		break;
	case XED_ICLASS_JS:
		cond = compute_cond(cond_type::s);
		break;

	default:
		throw std::runtime_error("unhandled cond jump instruction");
	}

	xed_uint_t instruction_length = xed_decoded_inst_get_length(xed_inst());
	auto fallthrough = builder().insert_add(builder().insert_read_pc()->val(), builder().insert_constant_u64(instruction_length)->val());

	int64_t branch_displacement = xed_decoded_inst_get_branch_displacement(xed_inst());
	uint64_t branch_target = branch_displacement + instruction_length;

	auto target = builder().insert_add(builder().insert_read_pc()->val(), builder().insert_constant_u64(branch_target)->val());

	builder().insert_write_pc(builder().insert_csel(cond->val(), target->val(), fallthrough->val())->val(), br_type::csel);
}
