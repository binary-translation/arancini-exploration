#pragma once

#include <arancini/util/logger.h>

#include <stdexcept>

namespace arancini::output::dynamic::arm64 {

// TODO: should throw after logging
struct arm64_exception : public std::runtime_error {
    template <typename... Args>
    arm64_exception(std::string_view format, Args&&... args):
        std::runtime_error(fmt::format(format,
                                       std::forward<Args>(args)...))
    {
    }
};

} // arancini::output::dynamic::arm64

