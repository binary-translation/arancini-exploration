#include <arancini/input/x86/x86-input-arch.h>
#if defined(ARCH_X86_64)
#include <arancini/output/x86/x86-output-engine.h>
#elif defined(ARCH_AARCH64)
#include <arancini/output/arm64/arm64-output-engine.h>
#endif
#include <arancini/ir/chunk.h>
#include <arancini/ir/dot-graph-generator.h>
#include <arancini/ir/ir-builder.h>
#include <arancini/output/mc/machine-code-allocator.h>
#include <arancini/output/output-engine.h>
#include <arancini/output/output-personality.h>
#include <arancini/runtime/dbt/translation-cache.h>
#include <arancini/runtime/dbt/translation-engine.h>
#include <arancini/runtime/dbt/translation.h>
#include <arancini/runtime/exec/execution-context.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace arancini::runtime::dbt;
using namespace arancini::runtime::exec;
using namespace arancini::output;
using namespace arancini::output::mc;
using namespace arancini::ir;

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

class dbt_ir_builder : public ir_builder {
public:
	dbt_ir_builder()
		: code_ptr_(nullptr)
		, code_size_(0)
		, alloc_size_(0)
	{
	}

	virtual void begin_chunk() override { emit8(0xcc); }

	virtual void end_chunk() override { }

	virtual void begin_packet(off_t address, const std::string &disassembly = "") override { }
	virtual packet_type end_packet() override
	{
		emit8(0xc3);
		return packet_type::end_of_block;
	}

	translation *create_translation() { return new translation(code_ptr_, code_size_); }

protected:
	virtual void insert_action(action_node *a) override { }

private:
	void ensure_capacity(size_t extra)
	{
		if ((code_size_ + extra) > alloc_size_) {
			if (alloc_size_ == 0) {
				alloc_size_ = 64;
			} else {
				alloc_size_ *= 2;
			}

			code_ptr_ = std::realloc(code_ptr_, alloc_size_);
		}
	}

	template <typename T> void emit(T v)
	{
		ensure_capacity(sizeof(T));
		((T *)code_ptr_)[code_size_] = v;
		code_size_ += sizeof(T);
	}

	void emit8(unsigned char c) { emit<decltype(c)>(c); }
	void emit16(unsigned short c) { emit<decltype(c)>(c); }
	void emit32(unsigned int c) { emit<decltype(c)>(c); }
	void emit64(unsigned long c) { emit<decltype(c)>(c); }

	void *code_ptr_;
	size_t code_size_;
	size_t alloc_size_;
};

translation *translation_engine::translate(unsigned long pc)
{
	void *code = ec_.get_memory_ptr(pc);

	dbt_ir_builder builder;
	ia_.translate_chunk(builder, pc, code, 0x1000, true);

	auto t = builder.create_translation();

	return t;
}
