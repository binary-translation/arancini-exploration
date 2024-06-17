#pragma once
#include <arancini/ir/value-type.h>
#ifdef NLIB
#include <arancini/native_lib/idl-ast-node.h>
#include <arancini/native_lib/native-lib-idl.h>
#endif
#include <arancini/native_lib/nlib_func.h>
#include <string>
#include <unordered_map>

namespace arancini::native_lib {
enum nlib_type_class { NLTC_VOID, NLTC_SINT, NLTC_UINT, NLTC_FLOAT, NLTC_STRING, NLTC_MEMPTR, NLTC_FNPTR, NLTC_FD, NLTC_CPLX };

class NativeLibs {
#ifdef NLIB
public:
	explicit NativeLibs(std::istream &in)
		: scanner_()
		, parser_(scanner_, *this)
	{
		scanner_.switch_streams(&in, NULL);
	}
	void add_function(const std::string &fname, std::string libname, ir::value_type type, std::vector<ir::value_type> vector);
	[[maybe_unused]] void idl_commit(idl_ast_node &root);

	/// Return true iff parsing successful
	bool parse() { return !parser_.parse(); }

	const std::unordered_map<std::string, nlib_function> &native_functions() const { return native_functions_; }

private:
	Scanner scanner_;
	Parser parser_;
	std::unordered_map<std::string, nlib_function> native_functions_;
	static ir::value_type_class nlib_tc_to_vt(nlib_type_class tc);
#endif
};

} // namespace arancini::native_lib
