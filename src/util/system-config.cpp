#include <arancini/util/system-config.h>

#include <arancini/util/logger.h>

#include <iostream>

using namespace util;

bool system_config::handle_enable_log() {
    const char* flag = getenv("ARANCINI_ENABLE_LOG");
    if (flag) {
        if (util::case_ignore_string_equal(flag, "true"))
            util::global_logger.enable(true);
        else if (util::case_ignore_string_equal(flag, "false"))
            util::global_logger.enable(false);
        else throw std::runtime_error("ARANCINI_ENABLE_LOG must be set to either true or false");
    }

    return util::global_logger.is_enabled();
}

bool system_config::handle_log_level() {
    const char* flag = getenv("ARANCINI_LOG_LEVEL");
    if (flag && util::global_logger.is_enabled()) {
        if (util::case_ignore_string_equal(flag, "debug"))
            util::global_logger.set_level(util::global_logging::levels::debug);
        else if (util::case_ignore_string_equal(flag, "info"))
            util::global_logger.set_level(util::global_logging::levels::info);
        else if (util::case_ignore_string_equal(flag, "warn"))
            util::global_logger.set_level(util::global_logging::levels::warn);
        else if (util::case_ignore_string_equal(flag, "error"))
            util::global_logger.set_level(util::global_logging::levels::error);
        else if (util::case_ignore_string_equal(flag, "fatal"))
            util::global_logger.set_level(util::global_logging::levels::fatal);
        else throw std::runtime_error("ARANCINI_LOG_LEVEL must be set to one among: debug, info, warn, error or fatal (case-insensitive)");

        return true;
    } else if (util::global_logger.is_enabled()) {
        std::cerr << "Logger enabled without explicit log level; setting log level to default [info]\n";
        util::global_logger.set_level(util::global_logging::levels::info);

        return false;
    }

    return false;
}

bool system_config::handle_log_stream() {
    const char* flag = getenv("ARANCINI_LOG_STREAM");
    if (flag && util::global_logger.is_enabled()) {
        if (util::case_ignore_string_equal(flag, "stdout"))
            util::global_logger.set_output_file(stdout);
        else if (util::case_ignore_string_equal(flag, "stderr"))
            util::global_logger.set_output_file(stderr);
        else {
            // Open file
            FILE* out = std::fopen(flag, "w");
            if (!out)
                throw std::runtime_error("Unable to open requested file for the Arancini logger stream");

            util::global_logger.set_output_file(out);
        }

        return true;
    } else if (util::global_logger.is_enabled()) {
        std::cerr << "Logger enabled without explicit log stream; the default log stream will be used [stderr]\n";
        return false;
    }

    return false;
}

bool system_config::handle_chaining() {
    const char* flag = getenv("ARANCINI_CHAIN");
    if (flag) {
        if (util::case_ignore_string_equal(flag, "true"))
            return (is_chaining_ = true);
        else if (util::case_ignore_string_equal(flag, "false"))
            return (is_chaining_ = false);
        throw std::runtime_error("ARANCINI_CHAIN may only be set to either true or false");
    }

    return is_chaining_;
}

bool system_config::handle_optimize_flags() {
    const char* flag = getenv("ARANCINI_OPTIMIZE_FLAGS");
    if (flag) {
        if (util::case_ignore_string_equal(flag, "true"))
            return (is_optimize_flags_ = true);
        else if (util::case_ignore_string_equal(flag, "false"))
            return (is_optimize_flags_ = false);
    }

    return is_optimize_flags_;
}

