#pragma once

#include <arancini/util/logger.h>

namespace arancini::output::dynamic::arm64 {

class backend_exception : public std::runtime_error {
public:
    template <typename... Args>
    backend_exception(std::string_view format, Args&&... args):
        std::runtime_error(fmt::format("[ARM64][EXCEPTION] {}",
                                       fmt::format(format, std::forward<Args>(args)...)))
    { }
};

inline auto& logger = util::global_logger;

} // arancini::output::dynamic::arm64

