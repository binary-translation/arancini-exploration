#include <arancini/native_lib/native-lib.h>
#include <arancini/native_lib/nlib_func.h>


namespace arancini::native_lib {

/**
 * Registers a function to be redirected to native code from a guest
 * shared-library invocation.
 */
void NativeLibs::add_function(const std::string &fname, std::string libname, ir::value_type type, std::vector<ir::value_type> vector)
{

	// Copy in the function details that we know at this time.
	nlib_function fn { fname, std::move(libname), { type, vector }};


	// Add the new function to the list.
	native_functions_.emplace(fname, std::move(fn));
}
ir::value_type_class NativeLibs::nlib_tc_to_vt(nlib_type_class tc)
{
	switch (tc) {
	case NLTC_VOID:
		return ir::value_type_class::none;
	case NLTC_SINT:
		return ir::value_type_class::signed_integer;
	case NLTC_UINT:
	case NLTC_STRING: // uintptr_t
	case NLTC_MEMPTR: // uintptr_t
	case NLTC_CPLX: // uintptr_t
		return ir::value_type_class::unsigned_integer;
	case NLTC_FLOAT:
		return ir::value_type_class::floating_point;
	case NLTC_FNPTR:
	case NLTC_FD:
	default:
		throw std::runtime_error("Unsupported arg type for nlib def");
	}
}

} // namespace arancini::native_lib