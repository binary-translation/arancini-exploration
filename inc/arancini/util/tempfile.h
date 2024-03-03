#pragma once

#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

namespace arancini::util {
class basefile {
public:
	basefile(const std::string &name)
		: name_(name) {}
	const std::string &name() const { return name_; }
	virtual std::ofstream open() = 0;

protected:
	std::string name_;
};

class tempfile : public basefile {
public:
	tempfile(const std::string &name) : basefile(name) {};
	~tempfile() { unlink(name_.c_str()); }

	std::ofstream open() override { return std::ofstream(name_); }
};

class persfile : public basefile {
public:
	persfile(const std::string &name) : basefile(name) {};

	std::ofstream open() override { return std::ofstream(name_);}

private:
};
} // namespace arancini::util
