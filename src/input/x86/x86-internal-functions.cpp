#include <arancini/input/x86/x86-internal-functions.h>
#include <arancini/ir/node.h>

using namespace arancini::input::x86;
using namespace arancini::ir;

std::shared_ptr<internal_function> x86_internal_functions::create(const std::string &name) const
{
	if (name == "handle_int") {
		return std::make_shared<internal_function>("handle_int", function_type(value_type::v(), { value_type::u32() }));
	} else if (name == "handle_syscall") {
		return std::make_shared<internal_function>("handle_syscall", function_type(value_type::v(), {}));
	} else if (name == "handle_poison") {
		return std::make_shared<internal_function>("handle_poison", function_type(value_type::v(), { value_type::v() }));
	} else if (name == "hlt") {
		return std::make_shared<internal_function>("hlt", function_type(value_type::v(), {}));
	}
	return nullptr;
}
