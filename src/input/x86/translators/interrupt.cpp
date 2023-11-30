#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/internal-function-resolver.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void interrupt_translator::do_translate()
{
	// auto ihf =
	switch (xed_decoded_inst_get_iclass(xed_inst())) {
	case XED_ICLASS_INT: {
		builder().insert_internal_call(
			builder().ifr().resolve("handle_int"), { &builder().insert_constant_u32(xed_decoded_inst_get_unsigned_immediate(xed_inst()))->val() });
		break;
	}

	case XED_ICLASS_INT3: {
		builder().insert_internal_call(builder().ifr().resolve("handle_int"), { &builder().insert_constant_u32(3)->val() });
		break;
	}

	case XED_ICLASS_SYSCALL: {
		auto next = builder().insert_add(builder().insert_read_pc()->val(), builder().insert_constant_u64(2)->val());
		write_reg(reg_offsets::PC, next->val());
		builder().insert_internal_call(builder().ifr().resolve("handle_syscall"), {});
		break;
	}

	case XED_ICLASS_UD0:
	case XED_ICLASS_UD1:
	case XED_ICLASS_UD2: {
		// Raises an 'Invalid Opcode' exception (exception number: 6)
		builder().insert_internal_call(builder().ifr().resolve("handle_int"), { &builder().insert_constant_u32(6)->val() });
		break;
	}

	default:
		throw std::runtime_error("unsupported interrupt operation");
	}
}
