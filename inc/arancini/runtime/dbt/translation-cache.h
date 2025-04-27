#pragma once

#include <unordered_map>

namespace arancini::runtime::dbt {
class translation;

class translation_cache {
  public:
    bool lookup(unsigned long addr, translation *&obj) {
        auto t = translations_.find(addr);
        if (t == translations_.end()) {
            return false;
        }

        obj = t->second;
        return true;
    }

    void insert(unsigned long addr, translation *obj) {
        translations_[addr] = obj;
    }

  private:
    std::unordered_map<unsigned long, translation *> translations_;
};
} // namespace arancini::runtime::dbt
