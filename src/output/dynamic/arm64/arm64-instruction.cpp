#include <arancini/output/dynamic/arm64/arm64-instruction.h>

using namespace arancini::output::dynamic::arm64;

std::size_t assembler::assemble(const char *code, unsigned char **out) {
    std::size_t size = 0;
    std::size_t count = 0;
    if (ks_asm(ks_, code, 0, out, &size, &count))
        throw backend_exception(
            "Keystone assembler encountered error after {} instructions: {}",
            count, ks_strerror(ks_errno(ks_)));

    return size;
}
