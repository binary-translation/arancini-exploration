#pragma once

#include <arancini/output/dynamic/machine-code-allocator.h>
#include <arancini/runtime/dbt/translation-cache.h>

namespace arancini::input {
class input_arch;
}

namespace arancini::output::dynamic {
class dynamic_output_engine;
}

namespace arancini::runtime::exec {
class execution_context;
}

namespace arancini::runtime::dbt {
using exec::execution_context;

class translation;

class translation_engine {
public:
	translation_engine(execution_context &ec, input::input_arch &ia, output::dynamic::dynamic_output_engine &oe)
		: ec_(ec)
		, ia_(ia)
		, oe_(oe)
		, code_arena_(0x100000000), alloc_ {code_arena_}
	{
	}

	translation *get_translation(unsigned long pc);
	translation *translate(unsigned long pc);

private:
	execution_context &ec_;
	translation_cache cache_;
	output::dynamic::arena code_arena_;
	output::dynamic::arena_machine_code_allocator alloc_;

	input::input_arch &ia_;
	output::dynamic::dynamic_output_engine &oe_;
};
} // namespace arancini::runtime::dbt
