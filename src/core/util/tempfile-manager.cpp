#include <arancini/util/tempfile-manager.h>
#include <arancini/util/tempfile.h>
#include <cstdio>

using namespace arancini::util;

std::shared_ptr<tempfile> tempfile_manager::create_file(const std::string &suffix)
{
	const char *n = tempnam(nullptr, "T-");

	auto t = std::make_shared<tempfile>(std::string(n) + suffix);
	tempfiles_.push_back(t);

	return t;
}
