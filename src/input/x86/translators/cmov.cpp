#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void cmov_translator::do_translate() {
    auto xed_iclass = xed_decoded_inst_get_iclass(xed_inst());

    value_node *cond;
    switch (xed_iclass) {
    case XED_ICLASS_FCMOVNBE:
    case XED_ICLASS_CMOVNBE:
        cond = compute_cond(cond_type::nbe);
        break;
    case XED_ICLASS_FCMOVNB:
    case XED_ICLASS_CMOVNB:
        cond = compute_cond(cond_type::nb);
        break;
    case XED_ICLASS_FCMOVB:
    case XED_ICLASS_CMOVB:
        cond = compute_cond(cond_type::b);
        break;
    case XED_ICLASS_FCMOVBE:
    case XED_ICLASS_CMOVBE:
        cond = compute_cond(cond_type::be);
        break;
    case XED_ICLASS_FCMOVE:
    case XED_ICLASS_CMOVZ:
        cond = compute_cond(cond_type::z);
        break;
    case XED_ICLASS_CMOVNLE:
        cond = compute_cond(cond_type::nle);
        break;
    case XED_ICLASS_CMOVNL:
        cond = compute_cond(cond_type::nl);
        break;
    case XED_ICLASS_CMOVL:
        cond = compute_cond(cond_type::l);
        break;
    case XED_ICLASS_CMOVLE:
        cond = compute_cond(cond_type::le);
        break;
    case XED_ICLASS_FCMOVNE:
    case XED_ICLASS_CMOVNZ:
        cond = compute_cond(cond_type::nz);
        break;
    case XED_ICLASS_CMOVNO:
        cond = compute_cond(cond_type::no);
        break;
    case XED_ICLASS_FCMOVNU:
    case XED_ICLASS_CMOVNP:
        cond = compute_cond(cond_type::np);
        break;
    case XED_ICLASS_CMOVNS:
        cond = compute_cond(cond_type::ns);
        break;
    case XED_ICLASS_CMOVO:
        cond = compute_cond(cond_type::o);
        break;
    case XED_ICLASS_FCMOVU:
    case XED_ICLASS_CMOVP:
        cond = compute_cond(cond_type::p);
        break;
    case XED_ICLASS_CMOVS:
        cond = compute_cond(cond_type::s);
        break;

    default:
        throw std::runtime_error("unhandled cond mov instruction");
    }

    auto val = builder().insert_csel(cond->val(), read_operand(1)->val(),
                                     read_operand(0)->val());
    write_operand(0, val->val());

    // Handle X87 Tag Word
    switch (xed_iclass) {
    case XED_ICLASS_FCMOVE:
    case XED_ICLASS_FCMOVNE:
    case XED_ICLASS_FCMOVB:
    case XED_ICLASS_FCMOVBE:
    case XED_ICLASS_FCMOVNB:
    case XED_ICLASS_FCMOVNBE:
    case XED_ICLASS_FCMOVU:
    case XED_ICLASS_FCMOVNU: {
        // TODO: FPU: Underflow check

        // Get the stack index
        int st_idx_i = fpu_get_instruction_index(1);

        auto tag_val = builder().insert_csel(
            cond->val(), fpu_tag_get(st_idx_i)->val(), fpu_tag_get(0)->val());
        fpu_tag_set(0, tag_val->val());
    }
    default:
        break;
    }
}
