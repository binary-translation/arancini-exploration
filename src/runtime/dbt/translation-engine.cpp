#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/ir/chunk.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/output/mc/machine-code-allocator.h>
#include <arancini/output/output-engine.h>
#include <arancini/output/output-personality.h>
#include <arancini/runtime/dbt/translation-cache.h>
#include <arancini/runtime/dbt/translation-engine.h>
#include <arancini/runtime/dbt/translation.h>
#include <arancini/runtime/exec/execution-context.h>

#include <cstdlib>
#include <stdexcept>

using namespace arancini::runtime::dbt;
using namespace arancini::runtime::exec;
using namespace arancini::output;
using namespace arancini::output::mc;

class dbt_output_personality : public dynamic_output_personality {
public:
	dbt_output_personality()
		: allocator_(std::make_shared<default_machine_code_allocator>())
	{
	}

	virtual std::shared_ptr<machine_code_allocator> get_allocator() const override { return allocator_; }

private:
	std::shared_ptr<machine_code_allocator> allocator_;
};

translation *translation_engine::get_translation(unsigned long pc)
{
	translation *t;
	if (!cache_.lookup(pc, t)) {
		t = translate(pc);
		if (!t) {
			throw std::runtime_error("translation failed");
		}

		cache_.insert(pc, t);
	}

	return t;
}

translation *translation_engine::translate(unsigned long pc)
{
	void *code = ec_.get_memory_ptr(pc);

	auto chunk = ia_.translate_chunk(pc, code, 0x1000, true);
	oe_.add_chunk(chunk);

	dbt_output_personality dop;
	oe_.generate(dop);

	return nullptr;
}
