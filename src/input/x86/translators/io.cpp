#include <arancini/input/x86/translators/translators.h>
#include <arancini/ir/internal-function-resolver.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/ir/node.h>

using namespace arancini::ir;
using namespace arancini::input::x86::translators;

void io_translator::do_translate() {
    switch (xed_decoded_inst_get_iclass(xed_inst())) {
    default:
        throw std::runtime_error("unsupported io operation");
    }
}
