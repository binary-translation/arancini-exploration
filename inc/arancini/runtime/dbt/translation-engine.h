#pragma once

#include <arancini/runtime/dbt/translation-cache.h>

namespace arancini::runtime::exec {
class execution_context;
}

namespace arancini::runtime::dbt {
using exec::execution_context;

class translation;

class translation_engine {
public:
	translation_engine(execution_context &ec)
		: ec_(ec)
	{
	}

	translation *get_translation(unsigned long pc);
	translation *translate(unsigned long pc);

private:
	execution_context &ec_;
	translation_cache cache_;
};
} // namespace arancini::runtime::dbt
