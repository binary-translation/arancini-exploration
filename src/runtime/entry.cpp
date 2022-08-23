#include <cstdlib>
#include <iostream>

extern "C" void initialise_dynamic_runtime() { std::cerr << "arancini: dbt: initialise" << std::endl; }

extern "C" void invoke_code(void *cpu_state)
{
	std::cerr << "arancini: dbt: invoke " << std::hex << cpu_state << std::endl;
	abort();
}
