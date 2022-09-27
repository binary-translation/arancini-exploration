#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/ir/chunk.h>
#include <arancini/runtime/dbt/translation-cache.h>
#include <arancini/runtime/dbt/translation-engine.h>
#include <arancini/runtime/dbt/translation.h>
#include <arancini/runtime/exec/execution-context.h>

#include <stdexcept>

using namespace arancini::runtime::dbt;
using namespace arancini::runtime::exec;

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
	void *code = (void *)((uintptr_t)ec_.get_memory() + pc);

	arancini::input::x86::x86_input_arch ia;
	auto chunk = ia.translate_chunk(pc, code, 0x1000, true);

	arancini::ir::dot_graph_generator dgg(std::cerr);
	chunk->accept(dgg);

	return nullptr;
}
