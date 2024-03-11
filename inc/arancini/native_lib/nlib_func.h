#pragma once
#include <arancini/ir/value-type.h>
#include <arancini/native_lib/idl-ast-node.h>
#include <string>
#include <unordered_map>

namespace arancini::native_lib {
struct nlib_function {
	std::string fname;
	std::string libname;

	arancini::ir::function_type sig;
};
} // namespace arancini::native_lib