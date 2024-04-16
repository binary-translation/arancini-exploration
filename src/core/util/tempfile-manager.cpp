#include <arancini/util/tempfile-manager.h>
#include <arancini/util/tempfile.h>
#include <cstdio>

using namespace arancini::util;

std::shared_ptr<basefile> tempfile_manager::create_file(const std::string& prefix, const std::string& suffix)
{
	if (prefix == "") {
		const char *n = tempnam(nullptr, "T-");

		auto t = std::make_shared<tempfile>(std::string(n) + suffix);
		tempfiles_.push_back(t);

		return t;
	}
	char n[5];
	sprintf(n, "P-%d", rand()%1000);
	auto t = std::make_shared<persfile>(prefix + std::string(n) + suffix);
	tempfiles_.push_back(t);

	return t;
}
