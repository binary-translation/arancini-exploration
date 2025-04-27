#include "arancini/ir/value-type.h"
#include <arancini/native_lib/native-lib-idl.h>
#include <arancini/native_lib/native-lib.h>

namespace arancini::native_lib {

[[maybe_unused]] void NativeLibs::idl_commit(idl_ast_node &root) {
    idl_ast_node &defs = root.children[0];

    std::string curlib;
    for (int i = 0; i < defs.nr_children; i++) {
        idl_ast_node &def = defs.children[i];
        if (def.ty == IANT_LIBDEF) {
            curlib = def.value;
        } else if (def.ty == IANT_FNDEF) {

            if (def.nr_children == 0) {
                throw Parser::syntax_error(
                    location{}, "nlib: idl: invalid function definition\n");
            }

            idl_ast_node &rv = def.children[0];
            ir::value_type retty = {
                nlib_tc_to_vt(static_cast<nlib_type_class>(rv.tc)),
                static_cast<ir::value_type::size_type>(
                    rv.width)};              // is_const ignored
            std::vector<ir::value_type> args{};
            if (rv.tc == NLTC_CPLX) {
                retty = ir::value_type::v(); // No actual return value
                args.reserve(def.nr_children);
                args.emplace_back(
                    ir::value_type::u64());  // ptr for return value
            } else {
                args.reserve(def.nr_children - 1);
            }

            if (def.nr_children > 1) {
                idl_ast_node &params = def.children[1];
                for (int j = 0; j < params.nr_children; j++) {
                    idl_ast_node &arg_v = params.children[j].children[0];
                    args.emplace_back(
                        nlib_tc_to_vt(static_cast<nlib_type_class>(arg_v.tc)),
                        arg_v.width);
                }
            }

            add_function(def.value, curlib, retty, args);
        } else {
            throw Parser::syntax_error(location{},
                                       "nlib: idl: unknown definition type\n");
        }
    }
}
} // namespace arancini::native_lib
