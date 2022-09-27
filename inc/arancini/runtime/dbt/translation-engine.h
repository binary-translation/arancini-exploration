#pragma once

namespace arancini::runtime::dbt {
class translation;

class translation_engine {
public:
	translation *get_translation(unsigned long pc);
};
} // namespace arancini::runtime::dbt
