#include <arancini/input/x86/x86-internal-functions.h>
#include <arancini/ir/node.h>

using namespace arancini::input::x86;
using namespace arancini::ir;

internal_function *x86_internal_functions::create(const std::string &name) const
{
	if (name == "handle_int") {
		return new internal_function("handle_int", function_type(value_type::v(), { value_type::u32() }));
	} else if (name == "handle_syscall") {
		return new internal_function("handle_syscall", function_type(value_type::v(), {}));
	}
	return nullptr;
}
