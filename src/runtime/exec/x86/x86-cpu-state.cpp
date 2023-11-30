#include <arancini/runtime/exec/x86/x86-cpu-state.h>

#include <ostream>

namespace arancini::runtime::exec::x86 {

std::ostream &operator<<(std::ostream &os, const x86_cpu_state &s)
{
	os << std::hex;
	os << "Program counter: 0x" << s.PC << '\n';
	os << "Registers:\n";
	os << "RAX:             0x" << s.RAX << '\n';
	os << "RBX:             0x" << s.RBX << '\n';
	os << "RCX:             0x" << s.RCX << '\n';
	os << "RDX:             0x" << s.RDX << '\n';
	os << "RSI:             0x" << s.RSI << '\n';
	os << "RDI:             0x" << s.RDI << '\n';
	os << "RBP:             0x" << s.RBP << '\n';
	os << "RSP:             0x" << s.RSP << '\n';
	os << "R8:              0x" << s.R8 << '\n';
	os << "R9:              0x" << s.R9 << '\n';
	os << "R10:             0x" << s.R10 << '\n';
	os << "R11:             0x" << s.R11 << '\n';
	os << "R12:             0x" << s.R12 << '\n';
	os << "R13:             0x" << s.R13 << '\n';
	os << "R14:             0x" << s.R14 << '\n';
	os << "R15:             0x" << s.R15 << '\n';
	os << "flag ZF:         0x" << static_cast<unsigned>(s.ZF) << '\n';
	os << "flag CF:         0x" << static_cast<unsigned>(s.CF) << '\n';
	os << "flag OF:         0x" << static_cast<unsigned>(s.OF) << '\n';
	os << "flag SF:         0x" << static_cast<unsigned>(s.SF) << '\n';
	os << "flag PF:         0x" << static_cast<unsigned>(s.PF) << '\n';
	os << "flag DF:         0x" << static_cast<unsigned>(s.DF) << '\n';

	return os;
}

std::ostream &print_stack(std::ostream &os, const uint64_t *rsp, size_t byte_count)
{
	if (byte_count == 0)
		return os;

	os << std::hex;
	os << "0x" << *rsp << "\t <- rsp\n";
	for (std::size_t i = 1; i < byte_count; ++i) {
		os << "0x" << *(rsp + i) << '\n';
	}

	return os;
}

} // namespace arancini::runtime::exec::x86
