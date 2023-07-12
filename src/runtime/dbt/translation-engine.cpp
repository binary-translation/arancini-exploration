#include <arancini/input/x86/x86-input-arch.h>
#include <arancini/ir/chunk.h>
#include <arancini/ir/default-ir-builder.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/ir/opt.h>
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
	dbt_ir_builder(internal_function_resolver &ifr, std::shared_ptr<translation_context> tctx)
		: ir_builder(ifr)
		, tctx_(tctx)
		, is_eob_(false)
	{
	}

	virtual void begin_chunk() override { tctx_->begin_block(); }

	virtual void end_chunk() override { tctx_->end_block(); }

	virtual void begin_packet(off_t address, const std::string &disassembly = "") override
	{
		is_eob_ = false;
		tctx_->begin_instruction(address, disassembly);
	}

	virtual packet_type end_packet() override
	{
		tctx_->end_instruction();
		return is_eob_ ? packet_type::end_of_block : packet_type::normal;
	}

	translation *create_translation()
	{
		auto &writer = tctx_->writer();

		writer.finalise();
		auto *translation_p = new translation(writer.ptr(), writer.size());
		writer.reset();

		return translation_p;
	}

	virtual local_var *alloc_local(const value_type &type) override
	{
		auto lcl = new local_var(type);
		locals_.push_back(lcl);

		return lcl;
	}

protected:
	virtual void insert_action(action_node *a) override
	{
		if (a->updates_pc()) {
			is_eob_ = true;
		}

		tctx_->lower(a);
	}

private:
	std::shared_ptr<translation_context> tctx_;
	bool is_eob_;
	std::vector<local_var *> locals_;
};

class opt_dbt_ir_builder : public default_ir_builder {
public:
	opt_dbt_ir_builder(internal_function_resolver &ifr, std::shared_ptr<translation_context> tctx, deadflags_opt_visitor &deadflags, bool debug = false)
		: default_ir_builder(ifr, debug)
		, tctx_(std::move(tctx))
		, deadflags_ { deadflags }
	{
	}
	void end_chunk() override
	{
		default_ir_builder::end_chunk();

		get_chunk()->accept(deadflags_);

		tctx_->begin_block();

		for (const auto &p : get_chunk()->packets()) {

			tctx_->begin_instruction(p->address(), p->disassembly());

			for (auto a : p->actions()) {
				tctx_->lower(a);
			}

			tctx_->end_instruction();
		}

		tctx_->end_block();
	}

	translation *create_translation()
	{
		auto &writer = tctx_->writer();

		writer.finalise();
		auto *translation_p = new translation(writer.ptr(), writer.size());
		writer.reset();

		return translation_p;
	}

private:
	std::shared_ptr<translation_context> tctx_;
	deadflags_opt_visitor &deadflags_;
};

translation *translation_engine::translate(unsigned long pc)
{
	void *code = ec_.get_memory_ptr(pc);

	std::cerr << "translating pc=" << std::hex << pc << std::endl;

	if (deadflags_) {
		opt_dbt_ir_builder builder(ia_.get_internal_function_resolver(), ctx_, *deadflags_);
		ia_.translate_chunk(builder, pc, code, 0x1000, true);
		return builder.create_translation();
	}

	dbt_ir_builder builder(ia_.get_internal_function_resolver(), ctx_);
	ia_.translate_chunk(builder, pc, code, 0x1000, true);
	return builder.create_translation();
}
