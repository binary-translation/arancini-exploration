#pragma once

#include <arancini/ir/ir-builder.h>
#include <memory>
#include <stdexcept>

namespace arancini::ir {
class chunk;
class packet;
class internal_function_resolver;

class default_ir_builder : public ir_builder {
public:
	default_ir_builder(internal_function_resolver &ifr, bool debug = false)
		: ir_builder(ifr)
		, chunk_complete_(false)
		, debug_(debug)
	{
	}

	virtual void begin_chunk() override;
	virtual void end_chunk() override;

	virtual void begin_packet(off_t address, const std::string &disassembly = "") override;
	virtual packet_type end_packet() override;

	std::shared_ptr<chunk> get_chunk() const
	{
		if (current_chunk_ == nullptr || !chunk_complete_) {
			throw std::runtime_error("cannot get chunk when it is incomplete");
		}

		return current_chunk_;
	}

	virtual const local_var *alloc_local(const value_type &type) override;

protected:
	virtual void insert_action(std::shared_ptr<action_node> a) override;
	virtual void process_node(node *a) override;

private:
	bool chunk_complete_, debug_;
	std::shared_ptr<chunk> current_chunk_;
	std::shared_ptr<packet> current_pkt_;
};
} // namespace arancini::ir
