#pragma once

#include <arancini/ir/ir-builder.h>
#include <memory>
#include <stdexcept>

namespace arancini::ir {
class chunk;
class packet;

class default_ir_builder : public ir_builder {
public:
	default_ir_builder()
		: chunk_complete_(false)
	{
	}

	virtual void begin_chunk() override;
	virtual void end_chunk() override;

	virtual void begin_packet(off_t address, const std::string& disassembly = "") override;
	virtual void end_packet() override;

	std::shared_ptr<chunk> get_chunk() const
	{
		if (current_chunk_ == nullptr || !chunk_complete_) {
			throw std::runtime_error("cannot get chunk when it is incomplete");
		}

		return current_chunk_;
	}

protected:
	virtual void insert_action(action_node *a) override;

private:
	bool chunk_complete_;
	std::shared_ptr<chunk> current_chunk_;
	std::shared_ptr<packet> current_pkt_;
};
} // namespace arancini::ir
