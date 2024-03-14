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
	builder().insert_internal_call(builder().ifr().resolve("handle_poison"), {});
}
