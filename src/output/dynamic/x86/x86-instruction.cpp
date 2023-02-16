#include <arancini/output/dynamic/x86/x86-instruction.h>

using namespace arancini::output::dynamic::x86;

void x86_instruction::emit(machine_code_writer &writer) const
{
	writer.emit8(0xcc);
	// ri.emit(writer);
}
