#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/ir/chunk.h>
#include <arancini/ir/default-ir-builder.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/output/dynamic/dynamic-output-engine.h>
#include <arancini/output/dynamic/machine-code-writer.h>
#include <arancini/output/dynamic/translation-context.h>
#include <arancini/runtime/dbt/translation-cache.h>
#include <arancini/runtime/dbt/translation-engine.h>
#include <arancini/runtime/dbt/translation.h>
#include <arancini/runtime/exec/execution-context.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace arancini::runtime::dbt;
using namespace arancini::runtime::exec;
using namespace arancini::output::dynamic;
using namespace arancini::ir;

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

class dbt_ir_builder : public ir_builder {
public:
	dbt_ir_builder(std::shared_ptr<translation_context> tctx)
		: tctx_(tctx)
		, is_eob_(false)
	{
	}

	virtual void begin_chunk() override { tctx_->begin_block(); }

	virtual void end_chunk() override { tctx_->end_block(); }

	virtual void begin_packet(off_t address, const std::string &disassembly = "") override
	{
		std::cerr << "dbt: lowering: " << disassembly << std::endl;
		is_eob_ = false;

		tctx_->begin_instruction();
	}

	virtual packet_type end_packet() override
	{
		tctx_->end_instruction();
		return is_eob_ ? packet_type::end_of_block : packet_type::normal;
	}

	translation *create_translation()
	{
		writer_.finalise();
		return new translation(writer_.ptr(), writer_.size());
	}

protected:
	virtual void insert_action(action_node *a) override
	{
		if (a->updates_pc() || true) {
			is_eob_ = true;
		}

		tctx_->lower(a);
	}

private:
	std::shared_ptr<translation_context> tctx_;
	machine_code_writer writer_;
	bool is_eob_;
};

translation *translation_engine::translate(unsigned long pc)
{
	void *code = ec_.get_memory_ptr(pc);

	std::cerr << "translating pc=" << std::hex << pc << std::endl;

	machine_code_writer writer;
	auto ctx = oe_.create_translation_context(writer);

	dbt_ir_builder builder(ctx);
	ia_.translate_chunk(builder, pc, code, 0x1000, true);

	return builder.create_translation();
}
