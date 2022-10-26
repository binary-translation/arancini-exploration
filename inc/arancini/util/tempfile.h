#pragma once

#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

namespace arancini::util {
class tempfile {
public:
	tempfile(const std::string &name)
		: name_(name)
	{
	}

	~tempfile() { unlink(name_.c_str()); }

	const std::string &name() const { return name_; }

	std::ofstream open() { return std::ofstream(name_); }

private:
	std::string name_;
};
} // namespace arancini::util
