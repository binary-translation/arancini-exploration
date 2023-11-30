#pragma once

#include <list>
#include <memory>
#include <string>

namespace arancini::util {
class tempfile;

class tempfile_manager {
public:
	std::shared_ptr<tempfile> create_file(const std::string &suffix = ".tmp");

private:
	std::list<std::shared_ptr<tempfile>> tempfiles_;
};
} // namespace arancini::util
