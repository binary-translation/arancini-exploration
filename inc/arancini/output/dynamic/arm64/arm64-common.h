#pragma once

#include <arancini/util/logger.h>
#include <arancini/ir/value-type.h>

namespace arancini::output::dynamic::arm64 {

class backend_exception : public std::runtime_error {
public:
    template <typename... Args>
    backend_exception(std::string_view format, Args&&... args):
        std::runtime_error(fmt::format(format, std::forward<Args>(args)...))
    { }
};

namespace value_types {

static ir::value_type addr_type = ir::value_type::u64();

static ir::value_type base_type = ir::value_type::u64();

static ir::value_type u12 = ir::value_type(ir::value_type_class::unsigned_integer, 12, 1);

static ir::value_type u6 = ir::value_type(ir::value_type_class::unsigned_integer, 6, 1);

}

inline auto& logger = util::global_logger;

} // arancini::output::dynamic::arm64

