#include "arancini/ir/node.h"
#include <arancini/output/dynamic/arm64/arm64-instruction.h>

#include <cstdint>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

using namespace arancini::output::dynamic::arm64;

size_t assembler::assemble(const char *code, unsigned char **out) {
    size_t size = 0;
    size_t count = 0;
    if (ks_asm(ks_, code, 0, out, &size, &count)) {
        std::string msg("Keystone assembler encountered error after count: ");
        msg += std::to_string(count);
        msg += ": ";
        throw std::runtime_error(msg + ks_strerror(ks_errno(ks_)));
    }

    return size;
}

