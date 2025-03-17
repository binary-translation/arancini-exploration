#include <arancini/output/dynamic/arm64/arm64-assembler.h>

namespace arancini::output::dynamic::arm64 {

std::size_t arm64_assembler::assemble(const char *code, unsigned char **out) {
    std::size_t size = 0;
    std::size_t count = 0;
    int status = ks_asm(ks_, code, 0, out, &size, &count);

    [[unlikely]]
    if (status == -1)
        throw backend_exception("Keystone assembler encountered invalid instruction error");

    [[unlikely]]
    if (status != 0)
        throw backend_exception("Keystone assembler encountered error after {} instructions: {}",
                                count, ks_strerror(ks_errno(ks_)));

    return size;
}

} // namespace arancini::output::dynamic::arm64
