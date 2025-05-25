#pragma once

#include <list>
#include <memory>
#include <string>

namespace arancini::util {
class basefile;

class tempfile_manager {
  public:
    std::shared_ptr<basefile> create_file(const std::string &prefix,
                                          const std::string &suffix = ".tmp");

  private:
    std::list<std::shared_ptr<basefile>> tempfiles_;
};
} // namespace arancini::util
