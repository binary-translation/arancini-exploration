#pragma once

#include <arancini/ir/opt.h>
#include <arancini/output/dynamic/dynamic-output-engine.h>
#include <arancini/output/dynamic/machine-code-allocator.h>
#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/runtime/dbt/translation-cache.h>

#include <memory>

namespace arancini::input {
class input_arch;
}

namespace arancini::output::dynamic {
class translation_context;
}

namespace arancini::runtime::exec {
class execution_context;
}

namespace arancini::runtime::dbt {
using exec::execution_context;

class translation;

class translation_engine {
public:
	translation_engine(execution_context &ec, input::input_arch &ia, output::dynamic::dynamic_output_engine &oe, bool optimise = true)
		: ec_(ec)
		, ia_(ia)
		, oe_(oe)
		, code_arena_(0x100000000)
		, alloc_ { code_arena_ }
		, writer_ { alloc_ }
		, ctx_ { oe_.create_translation_context(writer_) }
	{
		// TODO properly add flag to disable
		if (optimise) {
			deadflags_ = std::make_unique<ir::deadflags_opt_visitor>();
		}
	}

	translation *get_translation(unsigned long pc);
	translation *translate(unsigned long pc);
	void chain(uint64_t chain_address, void *chain_target);

private:
	execution_context &ec_;
	translation_cache cache_;
	output::dynamic::arena code_arena_;
	output::dynamic::arena_machine_code_allocator alloc_;
	output::dynamic::machine_code_writer writer_;

	input::input_arch &ia_;
	output::dynamic::dynamic_output_engine &oe_;
	std::shared_ptr<output::dynamic::translation_context> ctx_;

	std::unique_ptr<ir::deadflags_opt_visitor> deadflags_;
};
} // namespace arancini::runtime::dbt
