#include <arancini/txlat/txlat-engine.h>
#include <iostream>

using namespace arancini::txlat;

int main(int argc, const char *argv[])
{
	if (argc < 2) {
		std::cerr << "error: usage: " << argv[0] << std::endl;
		return 1;
	}

	txlat_engine e;

	try {
		e.translate(argv[1]);
	} catch (const std::exception &e) {
		std::cerr << "translation error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
