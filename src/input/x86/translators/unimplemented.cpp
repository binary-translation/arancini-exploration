#include "arancini/ir/value-type.h"
#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/internal-function-resolver.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void unimplemented_translator::do_translate()
{

	auto inst_class = xed_decoded_inst_get_iclass(xed_inst());
	builder().insert_internal_call(builder().ifr().resolve("handle_poison"), { &builder().insert_label(xed_iclass_enum_t2str(inst_class))->val() });
}
