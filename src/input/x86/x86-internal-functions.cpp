#include <arancini/input/x86/x86-internal-functions.h>
#include <arancini/ir/node.h>

using namespace arancini::input::x86;
using namespace arancini::ir;

std::shared_ptr<internal_function>
x86_internal_functions::create(const std::string &name) const {
    if (name == "handle_int") {
        return std::make_shared<internal_function>(
            "handle_int", function_type(value_type::v(), {value_type::u32()}));
    } else if (name == "handle_syscall") {
        return std::make_shared<internal_function>(
            "handle_syscall", function_type(value_type::v(), {}));
    } else if (name == "handle_poison") {
        return std::make_shared<internal_function>(
            "handle_poison", function_type(value_type::v(), {value_type::v()}));
    } else if (name == "hlt") {
        return std::make_shared<internal_function>(
            "hlt", function_type(value_type::v(), {}));
    } else if (name == "sin") {
        return std::make_shared<internal_function>(
            "sin", function_type(value_type::f64(), {value_type::f64()}));
    } else if (name == "cos") {
        return std::make_shared<internal_function>(
            "cos", function_type(value_type::f64(), {value_type::f64()}));
    } else if (name == "tan") {
        return std::make_shared<internal_function>(
            "tan", function_type(value_type::f64(), {value_type::f64()}));
    } else if (name == "atan") {
        return std::make_shared<internal_function>(
            "atan", function_type(value_type::f64(), {value_type::f64()}));
    } else if (name == "pow") {
        return std::make_shared<internal_function>(
            "pow", function_type(value_type::f64(),
                                 {value_type::f64(), value_type::f64()}));
    } else if (name == "log2") {
        return std::make_shared<internal_function>(
            "log2", function_type(value_type::f64(), {value_type::f64()}));
    }
    return nullptr;
}
